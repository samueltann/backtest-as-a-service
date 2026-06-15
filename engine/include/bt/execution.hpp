#pragma once

#include <map>
#include <string>
#include <vector>

#include "bt/events.hpp"

namespace bt {

// Simulated broker. An order submitted on a symbol's bar T is filled at that
// symbol's bar T+1's open — a signal computed on T's close can't be executed at a
// price that was only knowable on T (lookahead-bias prevention), and the delay is
// tracked independently per symbol. Slippage moves the fill price against the
// trader; commission is charged in basis points of notional.
class SimulatedExecutionHandler {
public:
    SimulatedExecutionHandler(double commission_bps, double slippage_bps)
        : commission_rate_(commission_bps / 10000.0),
          slippage_rate_(slippage_bps / 10000.0) {}

    // Queue an order for execution at its symbol's next bar's open.
    void on_order(const OrderEvent& order) { pending_[order.symbol].push_back(order); }

    // Called at the start of a new bar for one symbol: fill everything queued
    // previously for that symbol.
    std::vector<FillEvent> on_new_bar(const std::string& symbol, const Bar& bar);

    bool has_pending() const;

private:
    double commission_rate_;
    double slippage_rate_;
    std::map<std::string, std::vector<OrderEvent>> pending_;
};

}  // namespace bt
