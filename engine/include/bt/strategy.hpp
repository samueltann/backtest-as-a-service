#pragma once

#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

#include "bt/events.hpp"

namespace bt {

// A single-symbol strategy consumes one symbol's bars one at a time and may emit
// a directional signal. It holds its own indicator state and has no access to
// future bars. The engine runs one instance per symbol (see BasketStrategy), so
// its is_long_ flag is naturally per-symbol.
class SingleSymbolStrategy {
public:
    virtual ~SingleSymbolStrategy() = default;
    virtual std::optional<SignalEvent> on_bar(const Bar& bar) = 0;

protected:
    // Emit a signal only when the desired position state changes, so the
    // portfolio doesn't receive a stream of duplicate Long signals.
    std::optional<SignalEvent> transition_to(bool want_long, const Bar& bar) {
        if (want_long == is_long_) return std::nullopt;
        is_long_ = want_long;
        return SignalEvent{bar.date, want_long ? SignalType::Long : SignalType::Exit, bar.symbol};
    }

private:
    bool is_long_ = false;
};

// A portfolio-level strategy sees every symbol's bar for the current date at once
// and emits zero or more symbol-tagged signals. This is what the engine drives;
// it enables cross-sectional strategies (rank/select across the universe).
class Strategy {
public:
    virtual ~Strategy() = default;
    virtual std::vector<SignalEvent> on_slice(const TimeSlice& slice) = 0;
};

// Runs an independent SingleSymbolStrategy instance per symbol, created lazily
// from a factory. This is how the existing single-symbol strategies run across a
// basket: identical logic, separate indicator state per symbol, one shared
// portfolio.
class BasketStrategy final : public Strategy {
public:
    explicit BasketStrategy(std::function<std::unique_ptr<SingleSymbolStrategy>()> factory)
        : factory_(std::move(factory)) {}

    std::vector<SignalEvent> on_slice(const TimeSlice& slice) override {
        std::vector<SignalEvent> out;
        for (const auto& [symbol, bar] : slice.bars) {  // std::map -> deterministic order
            auto it = units_.find(symbol);
            if (it == units_.end()) it = units_.emplace(symbol, factory_()).first;
            if (auto signal = it->second->on_bar(bar)) out.push_back(*signal);
        }
        return out;
    }

private:
    std::function<std::unique_ptr<SingleSymbolStrategy>()> factory_;
    std::map<std::string, std::unique_ptr<SingleSymbolStrategy>> units_;
};

// Creates a single-symbol strategy by id, validating params against its schema.
// Throws EngineError for unknown ids or invalid params.
std::unique_ptr<SingleSymbolStrategy> make_single_symbol_strategy(const std::string& id,
                                                                  const nlohmann::json& params);

// Creates a portfolio-level strategy by id. Single-symbol strategies are wrapped
// in a BasketStrategy. Throws EngineError for unknown ids or invalid params.
std::unique_ptr<Strategy> make_strategy(const std::string& id, const nlohmann::json& params);

// Machine-readable catalog of all strategies and their parameter schemas. The
// backend serves this to the frontend, so the engine is the single source of
// truth for what strategies exist and what parameters they take.
nlohmann::json strategy_catalog();

}  // namespace bt
