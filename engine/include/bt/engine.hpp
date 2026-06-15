#pragma once

#include <string>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

namespace bt {

// Backtest configuration, parsed and validated from the JSON the backend writes.
// A run operates on a universe of one or more symbols, each with its own CSV.
struct Config {
    std::string strategy_id;
    nlohmann::json strategy_params;

    // The universe: (symbol, data_file) pairs. from_json populates this from
    // either the multi-symbol "symbols" array or the legacy scalar fields below.
    std::vector<std::pair<std::string, std::string>> universe;

    // Legacy single-symbol convenience. Kept so existing callers/tests can set a
    // Config directly; run_backtest falls back to these when `universe` is empty.
    std::string symbol;
    std::string data_file;

    std::string start_date;
    std::string end_date;
    double initial_capital = 100000.0;
    double commission_bps = 0.0;
    double slippage_bps = 0.0;
    double position_fraction = 0.95;

    static Config from_json(const nlohmann::json& j);
};

// Runs the full event loop and returns the results document:
// { "symbols": [...], "strategy": {...}, "metrics": {...},
//   "equity_curve": [[date, equity], ...], "trades": [...] }
// "symbol" is also echoed when the universe has exactly one symbol.
nlohmann::json run_backtest(const Config& config);

}  // namespace bt
