#include "bt/metrics.hpp"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <limits>

#include "bt/data_handler.hpp"

namespace bt {

namespace {
constexpr double kTradingDaysPerYear = 252.0;
}

int days_between(const std::string& from, const std::string& to) {
    auto parse = [](const std::string& s) {
        int y = 0, m = 0, d = 0;
        if (std::sscanf(s.c_str(), "%d-%d-%d", &y, &m, &d) != 3)
            throw EngineError("invalid date: " + s);
        using namespace std::chrono;
        return sys_days{year{y} / m / d};
    };
    return static_cast<int>((parse(to) - parse(from)).count());
}

Metrics compute_metrics(const std::vector<EquityPoint>& equity_curve,
                        const std::vector<Trade>& trades,
                        double initial_capital) {
    Metrics m;
    m.num_trades = static_cast<int>(trades.size());
    if (equity_curve.empty()) return m;

    m.final_equity = equity_curve.back().equity;
    m.total_return = m.final_equity / initial_capital - 1.0;

    // CAGR over calendar time.
    double days = days_between(equity_curve.front().date, equity_curve.back().date);
    double years = days / 365.25;
    if (years > 0.0 && m.final_equity > 0.0)
        m.cagr = std::pow(m.final_equity / initial_capital, 1.0 / years) - 1.0;

    // Daily returns.
    std::vector<double> returns;
    returns.reserve(equity_curve.size());
    for (std::size_t i = 1; i < equity_curve.size(); ++i) {
        double prev = equity_curve[i - 1].equity;
        if (prev > 0.0) returns.push_back(equity_curve[i].equity / prev - 1.0);
    }

    if (returns.size() > 1) {
        double mean = 0.0;
        for (double r : returns) mean += r;
        mean /= static_cast<double>(returns.size());

        double var = 0.0, downside_sq = 0.0;
        for (double r : returns) {
            var += (r - mean) * (r - mean);
            if (r < 0.0) downside_sq += r * r;
        }
        var /= static_cast<double>(returns.size() - 1);  // sample variance
        double stddev = std::sqrt(var);
        double downside_dev = std::sqrt(downside_sq / static_cast<double>(returns.size()));

        if (stddev > 0.0) m.sharpe = mean / stddev * std::sqrt(kTradingDaysPerYear);
        if (downside_dev > 0.0) m.sortino = mean / downside_dev * std::sqrt(kTradingDaysPerYear);
    }

    // Max drawdown and its longest peak-to-recovery duration.
    double peak = equity_curve.front().equity;
    std::string peak_date = equity_curve.front().date;
    for (const auto& point : equity_curve) {
        if (point.equity >= peak) {
            int span = days_between(peak_date, point.date);
            if (span > m.max_drawdown_days) m.max_drawdown_days = span;
            peak = point.equity;
            peak_date = point.date;
        } else if (peak > 0.0) {
            double dd = 1.0 - point.equity / peak;
            if (dd > m.max_drawdown) m.max_drawdown = dd;
        }
    }
    // If still underwater at the end, count the unrecovered span too.
    int tail_span = days_between(peak_date, equity_curve.back().date);
    if (tail_span > m.max_drawdown_days) m.max_drawdown_days = tail_span;

    // Trade-level stats.
    if (!trades.empty()) {
        int wins = 0;
        double gross_profit = 0.0, gross_loss = 0.0;
        for (const auto& t : trades) {
            if (t.pnl > 0.0) { ++wins; gross_profit += t.pnl; }
            else gross_loss += -t.pnl;
        }
        m.win_rate = static_cast<double>(wins) / static_cast<double>(trades.size());
        if (gross_loss > 0.0) m.profit_factor = gross_profit / gross_loss;
        else if (gross_profit > 0.0) m.profit_factor = std::numeric_limits<double>::infinity();
    }

    return m;
}

nlohmann::json metrics_to_json(const Metrics& m) {
    double pf = std::isinf(m.profit_factor) ? -1.0 : m.profit_factor;  // JSON has no Infinity
    return {
        {"total_return", m.total_return},
        {"cagr", m.cagr},
        {"sharpe", m.sharpe},
        {"sortino", m.sortino},
        {"max_drawdown", m.max_drawdown},
        {"max_drawdown_days", m.max_drawdown_days},
        {"win_rate", m.win_rate},
        {"profit_factor", pf},
        {"num_trades", m.num_trades},
        {"final_equity", m.final_equity},
    };
}

}  // namespace bt
