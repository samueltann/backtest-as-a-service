#include "bt/portfolio.hpp"

#include <cmath>

namespace bt {

Portfolio::Portfolio(double initial_capital, double position_fraction)
    : initial_capital_(initial_capital),
      position_fraction_(position_fraction),
      cash_(initial_capital) {}

std::optional<OrderEvent> Portfolio::on_signal(const SignalEvent& signal) {
    if (signal.type == SignalType::Long) {
        if (position_ > 0 || last_close_ <= 0.0) return std::nullopt;
        double equity = cash_ + position_ * last_close_;
        long qty = static_cast<long>(std::floor(equity * position_fraction_ / last_close_));
        if (qty <= 0) return std::nullopt;
        return OrderEvent{signal.date, OrderSide::Buy, qty};
    }
    // Exit: close the whole position.
    if (position_ <= 0) return std::nullopt;
    return OrderEvent{signal.date, OrderSide::Sell, position_};
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
        position_ += qty;
        open_entry_date_ = fill.date;
        open_entry_cost_ = cost;
        open_entry_qty_ = qty;
    } else {
        double proceeds = fill.quantity * fill.price - fill.commission;
        cash_ += proceeds;
        position_ -= fill.quantity;
        if (position_ == 0 && open_entry_qty_ > 0) {
            Trade t;
            t.entry_date = open_entry_date_;
            t.exit_date = fill.date;
            t.quantity = open_entry_qty_;
            t.entry_price = open_entry_cost_ / open_entry_qty_;
            t.exit_price = proceeds / fill.quantity;
            t.pnl = proceeds - open_entry_cost_;
            t.return_pct = open_entry_cost_ > 0.0 ? t.pnl / open_entry_cost_ : 0.0;
            trades_.push_back(t);
            open_entry_qty_ = 0;
        }
    }
}

void Portfolio::mark_to_market(const Bar& bar) {
    last_close_ = bar.close;
    equity_curve_.push_back({bar.date, cash_ + position_ * bar.close});
}

}  // namespace bt
