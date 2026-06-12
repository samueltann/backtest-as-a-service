#include "bt/engine.hpp"

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

void validate_date(const std::string& date, const char* field) {
    if (date.size() != 10 || date[4] != '-' || date[7] != '-')
        throw EngineError(std::string(field) + " must be YYYY-MM-DD, got: " + date);
}

}  // namespace

Config Config::from_json(const json& j) {
    Config c;
    try {
        c.strategy_id = j.at("strategy").at("id").get<std::string>();
        c.strategy_params = j.at("strategy").value("params", json::object());
        c.symbol = j.at("symbol").get<std::string>();
        c.data_file = j.at("data_file").get<std::string>();
        c.start_date = j.at("start_date").get<std::string>();
        c.end_date = j.at("end_date").get<std::string>();
        c.initial_capital = j.value("initial_capital", 100000.0);
        c.commission_bps = j.value("commission_bps", 0.0);
        c.slippage_bps = j.value("slippage_bps", 0.0);
        c.position_fraction = j.value("position_fraction", 0.95);
    } catch (const json::exception& e) {
        throw EngineError(std::string("invalid config: ") + e.what());
    }

    validate_date(c.start_date, "start_date");
    validate_date(c.end_date, "end_date");
    if (c.start_date >= c.end_date) throw EngineError("start_date must be before end_date");
    if (c.initial_capital <= 0.0) throw EngineError("initial_capital must be positive");
    if (c.position_fraction <= 0.0 || c.position_fraction > 1.0)
        throw EngineError("position_fraction must be in (0, 1]");
    if (c.commission_bps < 0.0 || c.slippage_bps < 0.0)
        throw EngineError("commission_bps and slippage_bps must be non-negative");
    return c;
}

json run_backtest(const Config& config) {
    CsvDataHandler data(config.data_file, config.start_date, config.end_date);
    auto strategy = make_strategy(config.strategy_id, config.strategy_params);
    Portfolio portfolio(config.initial_capital, config.position_fraction);
    SimulatedExecutionHandler execution(config.commission_bps, config.slippage_bps);
    EventQueue queue;

    // Event loop. Per bar:
    //   1. Orders queued on the previous bar fill at this bar's open.
    //   2. The strategy sees the new bar (MarketEvent) and may emit a signal,
    //      which chains Signal -> Order inside the queue; the order waits for
    //      the NEXT bar's open.
    //   3. Equity is recorded at this bar's close.
    while (auto bar = data.next()) {
        portfolio.update_price(*bar);
        for (auto& fill : execution.on_new_bar(*bar)) queue.push(fill);
        queue.push(MarketEvent{*bar});

        while (!queue.empty()) {
            Event event = std::move(queue.front());
            queue.pop();
            std::visit(
                overloaded{
                    [&](const MarketEvent& e) {
                        if (auto signal = strategy->on_market(e)) queue.push(*signal);
                    },
                    [&](const SignalEvent& e) {
                        if (auto order = portfolio.on_signal(e)) queue.push(*order);
                    },
                    [&](const OrderEvent& e) { execution.on_order(e); },
                    [&](const FillEvent& e) { portfolio.on_fill(e); },
                },
                event);
        }

        portfolio.mark_to_market(*bar);
    }

    Metrics metrics =
        compute_metrics(portfolio.equity_curve(), portfolio.trades(), config.initial_capital);

    json equity = json::array();
    for (const auto& point : portfolio.equity_curve())
        equity.push_back({point.date, point.equity});

    json trades = json::array();
    for (const auto& t : portfolio.trades()) {
        trades.push_back({{"entry_date", t.entry_date},
                          {"exit_date", t.exit_date},
                          {"quantity", t.quantity},
                          {"entry_price", t.entry_price},
                          {"exit_price", t.exit_price},
                          {"pnl", t.pnl},
                          {"return_pct", t.return_pct}});
    }

    return {{"symbol", config.symbol},
            {"strategy", {{"id", config.strategy_id}, {"params", config.strategy_params}}},
            {"metrics", metrics_to_json(metrics)},
            {"equity_curve", equity},
            {"trades", trades}};
}

}  // namespace bt
