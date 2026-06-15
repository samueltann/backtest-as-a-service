#pragma once

#include <map>
#include <queue>
#include <string>
#include <variant>

namespace bt {

// One daily OHLCV bar for a single symbol. Dates are ISO "YYYY-MM-DD" strings,
// which compare correctly with operator< (lexicographic order == chronological).
struct Bar {
    std::string date;
    double open = 0.0;
    double high = 0.0;
    double low = 0.0;
    double close = 0.0;
    double volume = 0.0;
    std::string symbol;
};

// All bars across the universe that share one date. Only symbols that actually
// traded on this date are present. Sorted by symbol for deterministic iteration.
struct TimeSlice {
    std::string date;
    std::map<std::string, Bar> bars;
};

// A new time slice is available. Drives the Strategy, which sees every symbol's
// bar for the current date at once (enabling cross-sectional strategies).
struct MarketEvent {
    TimeSlice slice;
};

// Long-only for now; Short is a future addition (would need signed positions).
enum class SignalType { Long, Exit };

// Strategy's directional intent for one symbol. The Portfolio turns it into a
// sized order.
struct SignalEvent {
    std::string date;
    SignalType type = SignalType::Exit;
    std::string symbol;
};

enum class OrderSide { Buy, Sell };

// Sized market order for one symbol. The ExecutionHandler fills it at that
// symbol's NEXT bar's open to avoid lookahead bias (the signal is computed on
// this bar's close).
struct OrderEvent {
    std::string date;
    OrderSide side = OrderSide::Buy;
    long quantity = 0;
    std::string symbol;
};

// Executed trade for one symbol, including transaction costs.
struct FillEvent {
    std::string date;
    OrderSide side = OrderSide::Buy;
    long quantity = 0;
    double price = 0.0;       // includes slippage
    double commission = 0.0;
    std::string symbol;
};

using Event = std::variant<MarketEvent, SignalEvent, OrderEvent, FillEvent>;
using EventQueue = std::queue<Event>;

}  // namespace bt
