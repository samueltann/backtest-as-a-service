#include "bt/portfolio.hpp"

#include <cmath>

namespace bt {

Portfolio::Portfolio(double initial_capital, double position_fraction)
    : initial_capital_(initial_capital),
      position_fraction_(position_fraction),
      cash_(initial_capital) {}

long Portfolio::position(const std::string& symbol) const {
    auto it = positions_.find(symbol);
    return it == positions_.end() ? 0 : it->second;
}

double Portfolio::total_equity() const {
    double equity = cash_;
    for (const auto& [symbol, qty] : positions_) {
        auto it = last_close_.find(symbol);
        if (it != last_close_.end()) equity += qty * it->second;
    }
    return equity;
}

std::optional<OrderEvent> Portfolio::on_signal(const SignalEvent& signal) {
    long pos = position(signal.symbol);
    auto close_it = last_close_.find(signal.symbol);
    double last_close = close_it == last_close_.end() ? 0.0 : close_it->second;

    if (signal.type == SignalType::Long) {
        if (pos > 0 || last_close <= 0.0) return std::nullopt;
        // Size against current total equity; concurrent longs are clamped to
        // available cash at fill time (see on_fill), so the order they fill in
        // matters — the engine processes signals in a deterministic symbol order.
        double equity = total_equity();
        long qty = static_cast<long>(std::floor(equity * position_fraction_ / last_close));
        if (qty <= 0) return std::nullopt;
        return OrderEvent{signal.date, OrderSide::Buy, qty, signal.symbol};
    }
    // Exit: close the whole position in this symbol.
    if (pos <= 0) return std::nullopt;
    return OrderEvent{signal.date, OrderSide::Sell, pos, signal.symbol};
}

void Portfolio::on_fill(const FillEvent& fill) {
    if (fill.side == OrderSide::Buy) {
        // The order was sized at the signal bar's close but fills at the next
        // bar's open; if price gapped up the full quantity may no longer be
        // affordable. Clamp to what cash covers (a partial fill) so the
        // portfolio can never go cash-negative.
        long qty = fill.quantity;
        double cost_per_share = fill.price + (fill.quantity > 0
                                    ? fill.commission / static_cast<double>(fill.quantity)
                                    : 0.0);
        if (cost_per_share > 0.0) {
            long affordable = static_cast<long>(std::floor(cash_ / cost_per_share));
            if (affordable < qty) qty = affordable;
        }
        if (qty <= 0) return;

        double cost = qty * cost_per_share;
        cash_ -= cost;
        positions_[fill.symbol] += qty;
        open_[fill.symbol] = OpenLot{fill.date, cost, qty};
    } else {
        double proceeds = fill.quantity * fill.price - fill.commission;
        cash_ += proceeds;
        long& pos = positions_[fill.symbol];
        pos -= fill.quantity;
        auto open_it = open_.find(fill.symbol);
        if (pos == 0 && open_it != open_.end() && open_it->second.qty > 0) {
            const OpenLot& lot = open_it->second;
            Trade t;
            t.symbol = fill.symbol;
            t.entry_date = lot.entry_date;
            t.exit_date = fill.date;
            t.quantity = lot.qty;
            t.entry_price = lot.cost / lot.qty;
            t.exit_price = proceeds / fill.quantity;
            t.pnl = proceeds - lot.cost;
            t.return_pct = lot.cost > 0.0 ? t.pnl / lot.cost : 0.0;
            trades_.push_back(t);
            open_.erase(open_it);
        }
    }
}

void Portfolio::mark_to_market(const Date& date) {
    equity_curve_.push_back({date, total_equity()});
}

}  // namespace bt
