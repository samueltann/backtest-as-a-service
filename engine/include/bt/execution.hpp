#pragma once

#include <optional>
#include <vector>

#include "bt/events.hpp"

namespace bt {

// Simulated broker. Orders submitted on bar T are filled at bar T+1's open —
// a signal computed on T's close can't be executed at a price that was only
// knowable on T (lookahead-bias prevention). Slippage moves the fill price
// against the trader; commission is charged in basis points of notional.
class SimulatedExecutionHandler {
public:
    SimulatedExecutionHandler(double commission_bps, double slippage_bps)
        : commission_rate_(commission_bps / 10000.0),
          slippage_rate_(slippage_bps / 10000.0) {}

    // Queue an order for execution at the next bar's open.
    void on_order(const OrderEvent& order) { pending_.push_back(order); }

    // Called at the start of each new bar: fill everything queued previously.
    std::vector<FillEvent> on_new_bar(const Bar& bar);

    bool has_pending() const { return !pending_.empty(); }

private:
    double commission_rate_;
    double slippage_rate_;
    std::vector<OrderEvent> pending_;
};

}  // namespace bt
