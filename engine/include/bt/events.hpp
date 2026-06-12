#pragma once

#include <queue>
#include <string>
#include <variant>

namespace bt {

// One daily OHLCV bar. Dates are ISO "YYYY-MM-DD" strings, which compare
// correctly with operator< (lexicographic order == chronological order).
struct Bar {
    std::string date;
    double open = 0.0;
    double high = 0.0;
    double low = 0.0;
    double close = 0.0;
    double volume = 0.0;
};

// New bar available. Drives the Strategy.
struct MarketEvent {
    Bar bar;
};

enum class SignalType { Long, Exit };

// Strategy's directional intent. The Portfolio turns this into a sized order.
struct SignalEvent {
    std::string date;
    SignalType type = SignalType::Exit;
};

enum class OrderSide { Buy, Sell };

// Sized market order. The ExecutionHandler fills it at the NEXT bar's open
// to avoid lookahead bias (the signal is computed on this bar's close).
struct OrderEvent {
    std::string date;
    OrderSide side = OrderSide::Buy;
    long quantity = 0;
};

// Executed trade, including transaction costs.
struct FillEvent {
    std::string date;
    OrderSide side = OrderSide::Buy;
    long quantity = 0;
    double price = 0.0;       // includes slippage
    double commission = 0.0;
};

using Event = std::variant<MarketEvent, SignalEvent, OrderEvent, FillEvent>;
using EventQueue = std::queue<Event>;

}  // namespace bt
