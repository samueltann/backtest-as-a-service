#include <catch2/catch_amalgamated.hpp>

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <utility>
#include <vector>

#include "bt/data_handler.hpp"
#include "bt/engine.hpp"
#include "bt/execution.hpp"

using namespace bt;
using Catch::Approx;
using nlohmann::json;

namespace {

// (date, (open, close)); high/low are derived. Mirrors the helper in test_engine.
using BarSpec = std::pair<std::string, std::pair<double, double>>;

std::string write_csv(const std::string& name, const std::vector<BarSpec>& bars) {
    auto path = (std::filesystem::temp_directory_path() / name).string();
    std::ofstream out(path);
    out << "Date,Open,High,Low,Close,Volume\n";
    for (const auto& [date, prices] : bars) {
        auto [open, close] = prices;
        out << date << ',' << open << ',' << std::max(open, close) << ','
            << std::min(open, close) << ',' << close << ",1000\n";
    }
    return path;
}

// Builds contiguous daily bars 2024-01-01.. with open == close == each given close.
std::vector<BarSpec> from_closes(const std::vector<double>& closes) {
    std::vector<BarSpec> bars;
    for (std::size_t i = 0; i < closes.size(); ++i) {
        char date[11];
        std::snprintf(date, sizeof date, "2024-01-%02zu", i + 1);
        bars.push_back({date, {closes[i], closes[i]}});
    }
    return bars;
}

}  // namespace

TEST_CASE("MultiCsvDataHandler merges symbols by date and stamps the symbol") {
    auto a = write_csv("bt_ms_a.csv", {{"2024-01-01", {10, 10}},
                                       {"2024-01-02", {11, 11}},
                                       {"2024-01-03", {12, 12}}});
    auto b = write_csv("bt_ms_b.csv", {{"2024-01-02", {20, 20}},
                                       {"2024-01-03", {21, 21}},
                                       {"2024-01-04", {22, 22}}});
    MultiCsvDataHandler data({{"AAA", a}, {"BBB", b}}, "2024-01-01", "2024-12-31");

    auto s1 = data.next_slice();
    REQUIRE(s1.has_value());
    REQUIRE(s1->date == "2024-01-01");
    REQUIRE(s1->bars.size() == 1);
    REQUIRE(s1->bars.count("AAA") == 1);
    REQUIRE(s1->bars.at("AAA").symbol == "AAA");   // symbol stamped onto the Bar
    REQUIRE(s1->bars.at("AAA").close == Approx(10.0));

    auto s2 = data.next_slice();
    REQUIRE(s2->date == "2024-01-02");
    REQUIRE(s2->bars.size() == 2);                 // both symbols trade this date
    REQUIRE(s2->bars.at("BBB").close == Approx(20.0));

    auto s3 = data.next_slice();
    REQUIRE(s3->date == "2024-01-03");
    REQUIRE(s3->bars.size() == 2);

    auto s4 = data.next_slice();
    REQUIRE(s4->date == "2024-01-04");
    REQUIRE(s4->bars.size() == 1);                 // only BBB still trading
    REQUIRE(s4->bars.count("BBB") == 1);

    REQUIRE_FALSE(data.next_slice().has_value());
    std::remove(a.c_str());
    std::remove(b.c_str());
}

TEST_CASE("execution queues and fills orders independently per symbol") {
    SimulatedExecutionHandler exec(/*commission_bps=*/0.0, /*slippage_bps=*/0.0);
    exec.on_order(OrderEvent{"2024-01-01", OrderSide::Buy, 10, "AAA"});
    exec.on_order(OrderEvent{"2024-01-01", OrderSide::Buy, 20, "BBB"});

    // An AAA bar fills only AAA's order; BBB's stays queued.
    Bar aaa{"2024-01-02", 100.0, 100.0, 100.0, 100.0, 0.0, "AAA"};
    auto f1 = exec.on_new_bar("AAA", aaa);
    REQUIRE(f1.size() == 1);
    REQUIRE(f1[0].symbol == "AAA");
    REQUIRE(f1[0].quantity == 10);
    REQUIRE(exec.has_pending());

    // BBB fills later, at ITS own next bar's open.
    Bar bbb{"2024-01-05", 50.0, 50.0, 50.0, 50.0, 0.0, "BBB"};
    auto f2 = exec.on_new_bar("BBB", bbb);
    REQUIRE(f2.size() == 1);
    REQUIRE(f2[0].symbol == "BBB");
    REQUIRE(f2[0].price == Approx(50.0));
    REQUIRE_FALSE(exec.has_pending());
}

TEST_CASE("basket shares capital and marks absent symbols to their last close") {
    // AAA trades every day; BBB trades 01-01, 01-02, 01-04 (gap on 01-03) and its
    // close steps to 15 on 01-02. Buy & hold at 0.5 equity per name buys 500 of
    // each by 01-02 (cash -> 0). On 01-03 BBB has no bar, so its 500 shares must
    // be carried forward at the last close (15) when marking equity.
    auto a = write_csv("bt_ms_basket_a.csv", {{"2024-01-01", {10, 10}},
                                              {"2024-01-02", {10, 10}},
                                              {"2024-01-03", {10, 10}},
                                              {"2024-01-04", {10, 10}}});
    auto b = write_csv("bt_ms_basket_b.csv", {{"2024-01-01", {10, 10}},
                                              {"2024-01-02", {10, 15}},
                                              {"2024-01-04", {10, 10}}});

    Config c;
    c.universe = {{"AAA", a}, {"BBB", b}};
    c.strategy_id = "buy_hold";
    c.strategy_params = json::object();
    c.start_date = "2024-01-01";
    c.end_date = "2024-12-31";
    c.initial_capital = 10000.0;
    c.position_fraction = 0.5;

    json results = run_backtest(c);
    const auto& curve = results["equity_curve"];

    REQUIRE(results["symbols"].size() == 2);
    REQUIRE(curve.size() == 4);                                    // one point per union date
    REQUIRE(curve[0][1].get<double>() == Approx(10000.0));         // flat before any fill
    REQUIRE(curve[1][1].get<double>() == Approx(12500.0));         // 500*10 + 500*15
    REQUIRE(curve[2][0] == "2024-01-03");
    REQUIRE(curve[2][1].get<double>() == Approx(12500.0));         // BBB carried at 15 on its gap day
    std::remove(a.c_str());
    std::remove(b.c_str());
}

TEST_CASE("cross-sectional momentum holds the top name and rotates as leadership changes") {
    // top_k=1, lookback=2 (2-bar return). X is flat (never positive momentum).
    // Y leads early, Z takes over mid-series, then all momentum decays.
    //   Y momentum: +1.0 on 01-03,01-04, then 0  -> hold Y, exit on 01-05
    //   Z momentum: 0, then +2.0 on 01-05,01-06, then 0 -> enter 01-05, exit 01-07
    // Signals fill at each symbol's next open (T+1), so the completed round trips
    // are Y[01-04 -> 01-06] and Z[01-06 -> 01-08].
    auto x = write_csv("bt_ms_x.csv", from_closes({10, 10, 10, 10, 10, 10, 10, 10}));
    auto y = write_csv("bt_ms_y.csv", from_closes({10, 10, 20, 20, 20, 20, 20, 20}));
    auto z = write_csv("bt_ms_z.csv", from_closes({10, 10, 10, 10, 30, 30, 30, 30}));

    Config c;
    c.universe = {{"X", x}, {"Y", y}, {"Z", z}};
    c.strategy_id = "xs_momentum";
    c.strategy_params = {{"lookback", 2}, {"top_k", 1}};
    c.start_date = "2024-01-01";
    c.end_date = "2024-12-31";
    c.initial_capital = 100000.0;
    c.position_fraction = 0.95;

    json results = run_backtest(c);
    const auto& trades = results["trades"];

    REQUIRE(trades.size() == 2);
    REQUIRE(trades[0]["symbol"] == "Y");
    REQUIRE(trades[0]["entry_date"] == "2024-01-04");
    REQUIRE(trades[0]["exit_date"] == "2024-01-06");
    REQUIRE(trades[1]["symbol"] == "Z");
    REQUIRE(trades[1]["entry_date"] == "2024-01-06");
    REQUIRE(trades[1]["exit_date"] == "2024-01-08");
    std::remove(x.c_str());
    std::remove(y.c_str());
    std::remove(z.c_str());
}

TEST_CASE("legacy single-symbol Config and config JSON still work") {
    SECTION("run_backtest accepts the legacy scalar symbol/data_file") {
        auto csv = write_csv("bt_ms_legacy.csv", from_closes({10, 11, 12}));
        Config c;
        c.symbol = "LEG";
        c.data_file = csv;
        c.strategy_id = "buy_hold";
        c.strategy_params = json::object();
        c.start_date = "2024-01-01";
        c.end_date = "2024-12-31";
        c.initial_capital = 10000.0;
        c.position_fraction = 1.0;

        json results = run_backtest(c);
        REQUIRE(results["symbols"].size() == 1);
        REQUIRE(results["symbols"][0] == "LEG");
        REQUIRE(results["symbol"] == "LEG");           // backward-compatible echo
        REQUIRE(results["equity_curve"].size() == 3);
        std::remove(csv.c_str());
    }

    SECTION("from_json parses the multi-symbol form") {
        json j = {{"strategy", {{"id", "xs_momentum"}, {"params", {{"lookback", 2}, {"top_k", 1}}}}},
                  {"symbols", json::array({{{"symbol", "AAA"}, {"data_file", "a.csv"}},
                                           {{"symbol", "BBB"}, {"data_file", "b.csv"}}})},
                  {"start_date", "2024-01-01"}, {"end_date", "2024-12-31"}};
        Config c = Config::from_json(j);
        REQUIRE(c.universe.size() == 2);
        REQUIRE(c.universe[0].first == "AAA");
        REQUIRE(c.universe[1].second == "b.csv");
    }

    SECTION("from_json still accepts the legacy single-symbol form") {
        json j = {{"strategy", {{"id", "buy_hold"}, {"params", json::object()}}},
                  {"symbol", "AAA"}, {"data_file", "a.csv"},
                  {"start_date", "2024-01-01"}, {"end_date", "2024-12-31"}};
        Config c = Config::from_json(j);
        REQUIRE(c.universe.size() == 1);
        REQUIRE(c.universe[0].first == "AAA");
        REQUIRE(c.universe[0].second == "a.csv");
    }
}
