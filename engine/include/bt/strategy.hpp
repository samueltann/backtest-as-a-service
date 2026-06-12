#pragma once

#include <memory>
#include <optional>
#include <string>

#include <nlohmann/json.hpp>

#include "bt/events.hpp"

namespace bt {

// A strategy consumes one bar at a time and may emit a directional signal.
// It holds its own indicator state; it has no access to future bars.
class Strategy {
public:
    virtual ~Strategy() = default;
    virtual std::optional<SignalEvent> on_market(const MarketEvent& event) = 0;

protected:
    // Emit a signal only when the desired position state changes, so the
    // portfolio doesn't receive a stream of duplicate Long signals.
    std::optional<SignalEvent> transition_to(bool want_long, const std::string& date) {
        if (want_long == is_long_) return std::nullopt;
        is_long_ = want_long;
        return SignalEvent{date, want_long ? SignalType::Long : SignalType::Exit};
    }

private:
    bool is_long_ = false;
};

// Creates a strategy by id, validating params against its schema.
// Throws EngineError for unknown ids or invalid params.
std::unique_ptr<Strategy> make_strategy(const std::string& id, const nlohmann::json& params);

// Machine-readable catalog of all strategies and their parameter schemas.
// The backend serves this to the frontend, so the engine is the single
// source of truth for what strategies exist and what parameters they take.
nlohmann::json strategy_catalog();

}  // namespace bt
