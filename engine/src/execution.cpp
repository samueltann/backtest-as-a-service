#include "bt/execution.hpp"

namespace bt {

std::vector<FillEvent> SimulatedExecutionHandler::on_new_bar(const std::string& symbol,
                                                             const Bar& bar) {
    std::vector<FillEvent> fills;
    auto it = pending_.find(symbol);
    if (it == pending_.end() || it->second.empty()) return fills;

    fills.reserve(it->second.size());
    for (const auto& order : it->second) {
        double direction = (order.side == OrderSide::Buy) ? 1.0 : -1.0;
        FillEvent fill;
        fill.date = bar.date;
        fill.side = order.side;
        fill.quantity = order.quantity;
        fill.price = bar.open * (1.0 + direction * slippage_rate_);
        fill.commission = commission_rate_ * fill.quantity * fill.price;
        fill.symbol = symbol;
        fills.push_back(fill);
    }
    it->second.clear();
    return fills;
}

bool SimulatedExecutionHandler::has_pending() const {
    for (const auto& [symbol, orders] : pending_) {
        (void)symbol;
        if (!orders.empty()) return true;
    }
    return false;
}

}  // namespace bt
