#include <catch2/catch_amalgamated.hpp>

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "bt/data_handler.hpp"
#include "bt/engine.hpp"
#include "bt/execution.hpp"
#include "bt/strategy.hpp"

using namespace bt;
using Catch::Approx;
using nlohmann::json;

namespace {

// Writes a CSV of daily bars (high/low derived from open/close) and returns its path.
std::string write_csv(const std::string& name,
                      const std::vector<std::pair<std::string, std::pair<double, double>>>& bars) {
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

Config base_config(const std::string& data_file) {
    Config c;
    c.symbol = "TEST";
    c.data_file = data_file;
    c.start_date = "2024-01-01";
    c.end_date = "2024-12-31";
    c.initial_capital = 10000.0;
    c.commission_bps = 0.0;
    c.slippage_bps = 0.0;
    c.position_fraction = 1.0;
    return c;
}

}  // namespace

TEST_CASE("orders fill at the NEXT bar's open, never the signal bar's close") {
    // Buy & hold signals Long on bar 1 (close 110). A lookahead-biased engine
    // would fill 90 shares at 110; a correct one fills at bar 2's open (120),
    // where only 83 shares are affordable.
    auto csv = write_csv("bt_test_lookahead.csv", {
        {"2024-01-01", {100.0, 110.0}},
        {"2024-01-02", {120.0, 125.0}},
        {"2024-01-03", {130.0, 140.0}},
    });
    auto config = base_config(csv);
    config.strategy_id = "buy_hold";
    config.strategy_params = json::object();

    json results = run_backtest(config);
    const auto& curve = results["equity_curve"];

    REQUIRE(curve.size() == 3);
    REQUIRE(curve[0][1].get<double>() == Approx(10000.0));            // flat on signal bar
    REQUIRE(curve[1][1].get<double>() == Approx(40.0 + 83 * 125.0));  // 10415, filled @ 120
    REQUIRE(curve[2][1].get<double>() == Approx(40.0 + 83 * 140.0));  // 11660
    std::remove(csv.c_str());
}

TEST_CASE("SMA crossover round trip with hand-computed fills and PnL") {
    // Closes ramp 10..15 then fall to 6 (open == close). With fast=2/slow=3:
    // Long on bar 3 (first bar both SMAs are ready, 11.5 > 11), filled bar 4
    // open @ 13 (clamped from 833 to 769 affordable shares); the cross-down
    // happens on bar 8 (13 < 13.67), so the exit fills bar 9 open @ 10.
    std::vector<std::pair<std::string, std::pair<double, double>>> bars;
    std::vector<double> prices = {10, 11, 12, 13, 14, 15, 14, 12, 10, 8, 6};
    for (std::size_t i = 0; i < prices.size(); ++i) {
        char date[11];
        std::snprintf(date, sizeof date, "2024-01-%02zu", i + 1);
        bars.push_back({date, {prices[i], prices[i]}});
    }
    auto csv = write_csv("bt_test_sma.csv", bars);

    auto config = base_config(csv);
    config.strategy_id = "sma_crossover";
    config.strategy_params = {{"fast_period", 2}, {"slow_period", 3}};

    json results = run_backtest(config);
    const auto& trades = results["trades"];

    REQUIRE(trades.size() == 1);
    REQUIRE(trades[0]["entry_date"] == "2024-01-04");
    REQUIRE(trades[0]["exit_date"] == "2024-01-09");
    REQUIRE(trades[0]["quantity"].get<long>() == 769);
    REQUIRE(trades[0]["entry_price"].get<double>() == Approx(13.0));
    REQUIRE(trades[0]["exit_price"].get<double>() == Approx(10.0));
    REQUIRE(trades[0]["pnl"].get<double>() == Approx(769 * 10.0 - 769 * 13.0));

    REQUIRE(results["metrics"]["num_trades"].get<int>() == 1);
    REQUIRE(results["metrics"]["final_equity"].get<double>() == Approx(7693.0));
    REQUIRE(results["metrics"]["win_rate"].get<double>() == Approx(0.0));
    std::remove(csv.c_str());
}

TEST_CASE("Bollinger mean reversion enters below the band and exits at the mean") {
    // period=5, num_std=1. After bar 6 the window is [100,100,100,100,88]:
    // SMA 97.6, stddev 4.8, lower band 92.8 — close 88 breaches it, so enter
    // (fills bar 7 open). At bar 7 close 100 >= SMA 97.6, so exit (fills bar 8
    // open). Sizing uses the signal-bar close (88 -> 113 shares) but the fill
    // clamps to the 100 shares cash covers at the open price of 100.
    std::vector<std::pair<std::string, std::pair<double, double>>> bars;
    std::vector<double> prices = {100, 100, 100, 100, 100, 88, 100, 105};
    for (std::size_t i = 0; i < prices.size(); ++i) {
        char date[11];
        std::snprintf(date, sizeof date, "2024-01-%02zu", i + 1);
        bars.push_back({date, {prices[i], prices[i]}});
    }
    auto csv = write_csv("bt_test_bollinger.csv", bars);

    auto config = base_config(csv);
    config.strategy_id = "bollinger_meanrev";
    config.strategy_params = {{"period", 5}, {"num_std", 1.0}};

    json results = run_backtest(config);
    const auto& trades = results["trades"];

    REQUIRE(trades.size() == 1);
    REQUIRE(trades[0]["entry_date"] == "2024-01-07");
    REQUIRE(trades[0]["exit_date"] == "2024-01-08");
    REQUIRE(trades[0]["quantity"].get<long>() == 100);
    REQUIRE(trades[0]["entry_price"].get<double>() == Approx(100.0));
    REQUIRE(trades[0]["exit_price"].get<double>() == Approx(105.0));
    REQUIRE(trades[0]["pnl"].get<double>() == Approx(500.0));
    std::remove(csv.c_str());
}

TEST_CASE("execution applies slippage against the trader and bps commission") {
    SimulatedExecutionHandler exec(/*commission_bps=*/50.0, /*slippage_bps=*/10.0);
    exec.on_order(OrderEvent{"2024-01-01", OrderSide::Buy, 100});

    Bar bar{"2024-01-02", 50.0, 51.0, 49.0, 50.5, 0.0};
    auto fills = exec.on_new_bar(bar);

    REQUIRE(fills.size() == 1);
    REQUIRE(fills[0].price == Approx(50.0 * 1.001));                  // pay up on buys
    REQUIRE(fills[0].commission == Approx(0.005 * 100 * 50.0 * 1.001));
    REQUIRE_FALSE(exec.has_pending());

    exec.on_order(OrderEvent{"2024-01-02", OrderSide::Sell, 100});
    auto sell_fills = exec.on_new_bar(bar);
    REQUIRE(sell_fills[0].price == Approx(50.0 * 0.999));             // receive less on sells
}

TEST_CASE("invalid configs and data produce clear EngineError messages") {
    SECTION("unknown strategy") {
        REQUIRE_THROWS_AS(make_strategy("does_not_exist", json::object()), EngineError);
    }
    SECTION("missing data file") {
        auto config = base_config("/nonexistent/nowhere.csv");
        config.strategy_id = "buy_hold";
        REQUIRE_THROWS_AS(run_backtest(config), EngineError);
    }
    SECTION("date range with no bars") {
        auto csv = write_csv("bt_test_range.csv", {{"2024-06-01", {10.0, 10.0}}});
        auto config = base_config(csv);
        config.strategy_id = "buy_hold";
        config.start_date = "2020-01-01";
        config.end_date = "2020-12-31";
        REQUIRE_THROWS_AS(CsvDataHandler(csv, config.start_date, config.end_date), EngineError);
        std::remove(csv.c_str());
    }
    SECTION("config validation rejects inverted dates") {
        json j = {{"strategy", {{"id", "buy_hold"}, {"params", json::object()}}},
                  {"symbol", "X"}, {"data_file", "x.csv"},
                  {"start_date", "2024-12-31"}, {"end_date", "2024-01-01"}};
        REQUIRE_THROWS_AS(Config::from_json(j), EngineError);
    }
}
