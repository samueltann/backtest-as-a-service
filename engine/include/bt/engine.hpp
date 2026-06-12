#pragma once

#include <string>

#include <nlohmann/json.hpp>

namespace bt {

// Backtest configuration, parsed and validated from the JSON the backend writes.
struct Config {
    std::string strategy_id;
    nlohmann::json strategy_params;
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
// { "metrics": {...}, "equity_curve": [[date, equity], ...], "trades": [...] }
nlohmann::json run_backtest(const Config& config);

}  // namespace bt
