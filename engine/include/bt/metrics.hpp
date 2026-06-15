#pragma once

#include <vector>

#include <nlohmann/json.hpp>

#include "bt/date.hpp"
#include "bt/portfolio.hpp"

namespace bt {

// Performance metrics computed from the equity curve and trade log.
struct Metrics {
    double total_return = 0.0;        // final/initial - 1
    double cagr = 0.0;                // annualized by calendar time
    double sharpe = 0.0;              // annualized, rf = 0
    double sortino = 0.0;             // annualized, downside deviation
    double max_drawdown = 0.0;        // worst peak-to-trough, as a positive fraction
    int max_drawdown_days = 0;        // longest peak-to-recovery span (calendar days)
    double win_rate = 0.0;
    double profit_factor = 0.0;       // gross profit / gross loss
    int num_trades = 0;
    double final_equity = 0.0;
};

Metrics compute_metrics(const std::vector<EquityPoint>& equity_curve,
                        const std::vector<Trade>& trades,
                        double initial_capital);

nlohmann::json metrics_to_json(const Metrics& m);

// Calendar days between two dates (to - from). Exposed for testing.
int days_between(const Date& from, const Date& to);

}  // namespace bt
