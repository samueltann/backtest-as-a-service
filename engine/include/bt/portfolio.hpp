#pragma once

#include <optional>
#include <string>
#include <vector>

#include "bt/events.hpp"

namespace bt {

// A completed round-trip trade (entry to flat), used for trade-level metrics.
struct Trade {
    std::string entry_date;
    std::string exit_date;
    long quantity = 0;
    double entry_price = 0.0;   // average fill price including slippage
    double exit_price = 0.0;
    double pnl = 0.0;           // net of commissions
    double return_pct = 0.0;    // pnl / entry cost
};

struct EquityPoint {
    std::string date;
    double equity = 0.0;
};

// Tracks cash, the open position, the equity curve, and the trade log.
// Long-only, single symbol in v1. Sizes orders as a fixed fraction of
// current equity (position_fraction), valued at the signal bar's close.
class Portfolio {
public:
    Portfolio(double initial_capital, double position_fraction);

    // Called when each bar arrives, so signals emitted on this bar are sized
    // against its close (the price the strategy actually saw).
    void update_price(const Bar& bar) { last_close_ = bar.close; }

    // Signal -> sized market order (or nothing if already positioned as desired).
    std::optional<OrderEvent> on_signal(const SignalEvent& signal);

    // Apply an executed fill: move cash, update position, record round trips.
    void on_fill(const FillEvent& fill);

    // Record equity (cash + position marked at close) at the end of each bar.
    void mark_to_market(const Bar& bar);

    double initial_capital() const { return initial_capital_; }
    double cash() const { return cash_; }
    long position() const { return position_; }
    const std::vector<EquityPoint>& equity_curve() const { return equity_curve_; }
    const std::vector<Trade>& trades() const { return trades_; }

private:
    double initial_capital_;
    double position_fraction_;
    double cash_;
    long position_ = 0;
    double last_close_ = 0.0;

    // Open-trade bookkeeping for the trade log.
    std::string open_entry_date_;
    double open_entry_cost_ = 0.0;   // cash spent incl. commission
    long open_entry_qty_ = 0;

    std::vector<EquityPoint> equity_curve_;
    std::vector<Trade> trades_;
};

}  // namespace bt
