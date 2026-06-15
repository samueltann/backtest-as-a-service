#include "bt/engine.hpp"

#include <utility>
#include <vector>

#include "bt/data_handler.hpp"
#include "bt/execution.hpp"
#include "bt/metrics.hpp"
#include "bt/portfolio.hpp"
#include "bt/strategy.hpp"

namespace bt {

using nlohmann::json;

namespace {

// std::visit helper: dispatch each event type to its handler lambda.
template <class... Ts>
struct overloaded : Ts... {
    using Ts::operator()...;
};
template <class... Ts>
overloaded(Ts...) -> overloaded<Ts...>;

Date parse_config_date(const std::string& s, const char* field) {
    try {
        return Date::parse(s);
    } catch (const std::invalid_argument&) {
        throw EngineError(std::string(field) + " must be a valid date (YYYY-MM-DD), got: " + s);
    }
}

}  // namespace

Config Config::from_json(const json& j) {
    Config c;
    std::string start_str, end_str;
    try {
        c.strategy_id = j.at("strategy").at("id").get<std::string>();
        c.strategy_params = j.at("strategy").value("params", json::object());

        if (j.contains("symbols")) {
            // Multi-symbol form: [{"symbol": "...", "data_file": "..."}, ...].
            for (const auto& entry : j.at("symbols"))
                c.universe.emplace_back(entry.at("symbol").get<std::string>(),
                                        entry.at("data_file").get<std::string>());
        } else {
            // Legacy single-symbol form.
            c.symbol = j.at("symbol").get<std::string>();
            c.data_file = j.at("data_file").get<std::string>();
            c.universe.emplace_back(c.symbol, c.data_file);
        }

        start_str = j.at("start_date").get<std::string>();
        end_str = j.at("end_date").get<std::string>();
        c.initial_capital = j.value("initial_capital", 100000.0);
        c.commission_bps = j.value("commission_bps", 0.0);
        c.slippage_bps = j.value("slippage_bps", 0.0);
        c.position_fraction = j.value("position_fraction", 0.95);
    } catch (const json::exception& e) {
        throw EngineError(std::string("invalid config: ") + e.what());
    }

    if (c.universe.empty()) throw EngineError("config must list at least one symbol");
    c.start_date = parse_config_date(start_str, "start_date");
    c.end_date = parse_config_date(end_str, "end_date");
    if (c.start_date >= c.end_date) throw EngineError("start_date must be before end_date");
    if (c.initial_capital <= 0.0) throw EngineError("initial_capital must be positive");
    if (c.position_fraction <= 0.0 || c.position_fraction > 1.0)
        throw EngineError("position_fraction must be in (0, 1]");
    if (c.commission_bps < 0.0 || c.slippage_bps < 0.0)
        throw EngineError("commission_bps and slippage_bps must be non-negative");
    return c;
}

json run_backtest(const Config& config) {
    // Effective universe: prefer the explicit list; fall back to the legacy
    // scalar symbol/data_file (used by direct-construction callers and tests).
    std::vector<std::pair<std::string, std::string>> universe = config.universe;
    if (universe.empty() && !config.symbol.empty())
        universe.emplace_back(config.symbol, config.data_file);
    if (universe.empty()) throw EngineError("config must list at least one symbol");

    MultiCsvDataHandler data(universe, config.start_date, config.end_date);
    auto strategy = make_strategy(config.strategy_id, config.strategy_params);
    Portfolio portfolio(config.initial_capital, config.position_fraction);
    SimulatedExecutionHandler execution(config.commission_bps, config.slippage_bps);
    EventQueue queue;

    // Event loop. Per time slice (one date across the universe):
    //   1. Orders queued on each symbol's previous bar fill at this bar's open,
    //      and the latest close is recorded for sizing/mark-to-market.
    //   2. The strategy sees the whole slice (MarketEvent) and may emit signals,
    //      which chain Signal -> Order inside the queue; orders wait for the NEXT
    //      bar's open of their symbol.
    //   3. Combined equity is recorded at this slice's close.
    while (auto slice = data.next_slice()) {
        for (const auto& [symbol, bar] : slice->bars) {
            for (auto& fill : execution.on_new_bar(symbol, bar)) queue.push(fill);
            portfolio.update_price(bar);
        }
        queue.push(MarketEvent{*slice});

        while (!queue.empty()) {
            Event event = std::move(queue.front());
            queue.pop();
            std::visit(
                overloaded{
                    [&](const MarketEvent& e) {
                        for (auto& signal : strategy->on_slice(e.slice)) queue.push(signal);
                    },
                    [&](const SignalEvent& e) {
                        if (auto order = portfolio.on_signal(e)) queue.push(*order);
                    },
                    [&](const OrderEvent& e) { execution.on_order(e); },
                    [&](const FillEvent& e) { portfolio.on_fill(e); },
                },
                event);
        }

        portfolio.mark_to_market(slice->date);
    }

    Metrics metrics =
        compute_metrics(portfolio.equity_curve(), portfolio.trades(), config.initial_capital);

    json equity = json::array();
    for (const auto& point : portfolio.equity_curve())
        equity.push_back({point.date.to_string(), point.equity});

    json trades = json::array();
    for (const auto& t : portfolio.trades()) {
        trades.push_back({{"symbol", t.symbol},
                          {"entry_date", t.entry_date.to_string()},
                          {"exit_date", t.exit_date.to_string()},
                          {"quantity", t.quantity},
                          {"entry_price", t.entry_price},
                          {"exit_price", t.exit_price},
                          {"pnl", t.pnl},
                          {"return_pct", t.return_pct}});
    }

    json symbols = json::array();
    for (const auto& entry : universe) symbols.push_back(entry.first);

    json results = {{"symbols", symbols},
                    {"strategy", {{"id", config.strategy_id}, {"params", config.strategy_params}}},
                    {"metrics", metrics_to_json(metrics)},
                    {"equity_curve", equity},
                    {"trades", trades}};
    // Backward-compatible single-symbol echo for current consumers.
    if (symbols.size() == 1) results["symbol"] = symbols[0];
    return results;
}

}  // namespace bt
