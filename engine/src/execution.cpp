#include "bt/execution.hpp"

namespace bt {

std::vector<FillEvent> SimulatedExecutionHandler::on_new_bar(const Bar& bar) {
    std::vector<FillEvent> fills;
    fills.reserve(pending_.size());
    for (const auto& order : pending_) {
        double direction = (order.side == OrderSide::Buy) ? 1.0 : -1.0;
        FillEvent fill;
        fill.date = bar.date;
        fill.side = order.side;
        fill.quantity = order.quantity;
        fill.price = bar.open * (1.0 + direction * slippage_rate_);
        fill.commission = commission_rate_ * fill.quantity * fill.price;
        fills.push_back(fill);
    }
    pending_.clear();
    return fills;
}

}  // namespace bt
