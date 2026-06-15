#pragma once

#include <map>
#include <optional>
#include <string>
#include <vector>

#include "bt/date.hpp"
#include "bt/events.hpp"

namespace bt {

// A completed round-trip trade (entry to flat) for one symbol, used for
// trade-level metrics.
struct Trade {
    Date entry_date;
    Date exit_date;
    long quantity = 0;
    double entry_price = 0.0;   // average fill price including slippage
    double exit_price = 0.0;
    double pnl = 0.0;           // net of commissions
    double return_pct = 0.0;    // pnl / entry cost
    std::string symbol;
};

struct EquityPoint {
    Date date;
    double equity = 0.0;
};

// Tracks shared cash, per-symbol positions, the combined equity curve, and the
// trade log. Long-only. Sizes each order as a fixed fraction of current total
// equity (position_fraction), valued at the signal bar's close for that symbol;
// for an equal-weight basket of N symbols, set position_fraction ~= 1/N.
class Portfolio {
public:
    Portfolio(double initial_capital, double position_fraction);

    // Record the latest close for a symbol, so signals emitted on this slice are
    // sized against the price the strategy actually saw and mark-to-market uses
    // the most recent known price.
    void update_price(const Bar& bar) { last_close_[bar.symbol] = bar.close; }

    // Signal -> sized market order (or nothing if already positioned as desired).
    std::optional<OrderEvent> on_signal(const SignalEvent& signal);

    // Apply an executed fill: move cash, update that symbol's position, record
    // round trips.
    void on_fill(const FillEvent& fill);

    // Total equity = shared cash + every position marked at its latest close.
    double total_equity() const;

    // Record combined equity at the end of a slice. Symbols with no bar on this
    // date are carried forward at their last known close.
    void mark_to_market(const Date& date);

    double initial_capital() const { return initial_capital_; }
    double cash() const { return cash_; }
    long position(const std::string& symbol) const;
    const std::vector<EquityPoint>& equity_curve() const { return equity_curve_; }
    const std::vector<Trade>& trades() const { return trades_; }

private:
    // Open-trade bookkeeping for the trade log, per symbol.
    struct OpenLot {
        Date entry_date;
        double cost = 0.0;   // cash spent incl. commission
        long qty = 0;
    };

    double initial_capital_;
    double position_fraction_;
    double cash_;
    std::map<std::string, long> positions_;
    std::map<std::string, double> last_close_;
    std::map<std::string, OpenLot> open_;

    std::vector<EquityPoint> equity_curve_;
    std::vector<Trade> trades_;
};

}  // namespace bt
