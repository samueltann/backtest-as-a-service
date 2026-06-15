#include "bt/strategy.hpp"

#include <algorithm>
#include <cstddef>
#include <deque>
#include <set>
#include <utility>
#include <vector>

#include "bt/data_handler.hpp"
#include "bt/indicators.hpp"

namespace bt {

namespace {

using nlohmann::json;

// --- parameter validation helpers -------------------------------------------

int require_int(const json& params, const std::string& key, int min_val, int max_val) {
    if (!params.contains(key) || !params[key].is_number_integer())
        throw EngineError("missing or non-integer param: " + key);
    int v = params[key].get<int>();
    if (v < min_val || v > max_val)
        throw EngineError("param " + key + " out of range [" + std::to_string(min_val) +
                          ", " + std::to_string(max_val) + "]");
    return v;
}

double require_num(const json& params, const std::string& key, double min_val, double max_val) {
    if (!params.contains(key) || !params[key].is_number())
        throw EngineError("missing or non-numeric param: " + key);
    double v = params[key].get<double>();
    if (v < min_val || v > max_val)
        throw EngineError("param " + key + " out of range");
    return v;
}

// --- single-symbol strategies ------------------------------------------------

// Long when the fast SMA is above the slow SMA, flat otherwise.
class SmaCrossover final : public SingleSymbolStrategy {
public:
    SmaCrossover(int fast, int slow) : fast_(fast), slow_(slow) {
        if (fast >= slow) throw EngineError("fast_period must be < slow_period");
    }

    std::optional<SignalEvent> on_bar(const Bar& bar) override {
        fast_.update(bar.close);
        slow_.update(bar.close);
        if (!slow_.ready()) return std::nullopt;
        return transition_to(fast_.value() > slow_.value(), bar);
    }

private:
    Sma fast_;
    Sma slow_;
};

// Trend-following: long while the close is above its EMA.
class EmaMomentum final : public SingleSymbolStrategy {
public:
    explicit EmaMomentum(int period) : ema_(period) {}

    std::optional<SignalEvent> on_bar(const Bar& bar) override {
        ema_.update(bar.close);
        if (!ema_.ready()) return std::nullopt;
        return transition_to(bar.close > ema_.value(), bar);
    }

private:
    Ema ema_;
};

// Mean reversion: buy when the close drops below the lower Bollinger band,
// exit when it reverts back up to the middle band (the SMA).
class BollingerMeanReversion final : public SingleSymbolStrategy {
public:
    BollingerMeanReversion(int period, double num_std)
        : sma_(period), stddev_(period), num_std_(num_std) {}

    std::optional<SignalEvent> on_bar(const Bar& bar) override {
        sma_.update(bar.close);
        stddev_.update(bar.close);
        if (!sma_.ready()) return std::nullopt;
        double lower = sma_.value() - num_std_ * stddev_.value();
        // Enter below the lower band, exit on reversion to the mean. The base
        // class's transition_to is the single source of position state and
        // dedupes repeat signals, so no separate in_position_ flag is needed.
        if (bar.close < lower) return transition_to(true, bar);
        if (bar.close >= sma_.value()) return transition_to(false, bar);
        return std::nullopt;
    }

private:
    Sma sma_;
    RollingStdDev stddev_;
    double num_std_;
};

// Mean reversion on RSI: buy oversold, exit overbought.
class RsiReversion final : public SingleSymbolStrategy {
public:
    RsiReversion(int period, double oversold, double overbought)
        : rsi_(period), oversold_(oversold), overbought_(overbought) {
        if (oversold >= overbought) throw EngineError("oversold must be < overbought");
    }

    std::optional<SignalEvent> on_bar(const Bar& bar) override {
        rsi_.update(bar.close);
        if (!rsi_.ready()) return std::nullopt;
        if (rsi_.value() < oversold_) return transition_to(true, bar);
        if (rsi_.value() > overbought_) return transition_to(false, bar);
        return std::nullopt;
    }

private:
    Rsi rsi_;
    double oversold_;
    double overbought_;
};

// Benchmark: buy on the first bar and hold to the end.
class BuyAndHold final : public SingleSymbolStrategy {
public:
    std::optional<SignalEvent> on_bar(const Bar& bar) override {
        return transition_to(true, bar);
    }
};

// --- cross-sectional strategies ----------------------------------------------

// Long-only cross-sectional momentum. Each bar, rank every symbol by its trailing
// return over `lookback` bars and hold the `top_k` names with positive momentum,
// equal-weight. A symbol's decision depends on the others, which is exactly the
// capability the time-slice interface exists to provide.
class XsMomentum final : public Strategy {
public:
    XsMomentum(int lookback, int top_k) : lookback_(lookback), top_k_(top_k) {}

    std::vector<SignalEvent> on_slice(const TimeSlice& slice) override {
        const auto window = static_cast<std::size_t>(lookback_) + 1;

        // Update each present symbol's rolling close history.
        for (const auto& [symbol, bar] : slice.bars) {
            auto& hist = closes_[symbol];
            hist.push_back(bar.close);
            if (hist.size() > window) hist.pop_front();
        }

        // Rank symbols present today that have a full window and positive momentum.
        std::vector<std::pair<double, std::string>> ranked;  // (momentum, symbol)
        for (const auto& [symbol, bar] : slice.bars) {
            (void)bar;
            const auto& hist = closes_[symbol];
            if (hist.size() < window) continue;
            double past = hist.front();
            if (past <= 0.0) continue;
            double momentum = hist.back() / past - 1.0;
            if (momentum > 0.0) ranked.push_back({momentum, symbol});
        }
        // Highest momentum first; tie-break on symbol so the selection is stable.
        std::sort(ranked.begin(), ranked.end(), [](const auto& a, const auto& b) {
            if (a.first != b.first) return a.first > b.first;
            return a.second < b.second;
        });

        std::set<std::string> target;
        for (std::size_t i = 0; i < ranked.size() && i < static_cast<std::size_t>(top_k_); ++i)
            target.insert(ranked[i].second);

        // Emit Long for symbols entering the target set and Exit for those leaving
        // it. Walk the union of currently-held and target symbols in sorted order
        // so the signal stream (and thus cash-clamped sizing) is deterministic.
        std::set<std::string> universe = target;
        for (const auto& [symbol, held] : held_)
            if (held) universe.insert(symbol);

        std::vector<SignalEvent> out;
        for (const auto& symbol : universe) {
            bool want = target.count(symbol) > 0;
            if (want == held_[symbol]) continue;
            held_[symbol] = want;
            out.push_back(SignalEvent{slice.date, want ? SignalType::Long : SignalType::Exit, symbol});
        }
        return out;
    }

private:
    int lookback_;
    int top_k_;
    std::map<std::string, std::deque<double>> closes_;
    std::map<std::string, bool> held_;
};

}  // namespace

std::unique_ptr<SingleSymbolStrategy> make_single_symbol_strategy(const std::string& id,
                                                                  const json& params) {
    if (id == "sma_crossover") {
        int fast = require_int(params, "fast_period", 2, 500);
        int slow = require_int(params, "slow_period", 3, 1000);
        return std::make_unique<SmaCrossover>(fast, slow);
    }
    if (id == "ema_momentum") {
        return std::make_unique<EmaMomentum>(require_int(params, "period", 2, 1000));
    }
    if (id == "bollinger_meanrev") {
        int period = require_int(params, "period", 5, 500);
        double num_std = require_num(params, "num_std", 0.5, 5.0);
        return std::make_unique<BollingerMeanReversion>(period, num_std);
    }
    if (id == "rsi_reversion") {
        int period = require_int(params, "period", 2, 200);
        double oversold = require_num(params, "oversold", 1.0, 99.0);
        double overbought = require_num(params, "overbought", 1.0, 99.0);
        return std::make_unique<RsiReversion>(period, oversold, overbought);
    }
    if (id == "buy_hold") {
        return std::make_unique<BuyAndHold>();
    }
    throw EngineError("unknown strategy: " + id);
}

std::unique_ptr<Strategy> make_strategy(const std::string& id, const json& params) {
    // Native cross-sectional strategies see the whole universe at once.
    if (id == "xs_momentum") {
        int lookback = require_int(params, "lookback", 2, 500);
        int top_k = require_int(params, "top_k", 1, 50);
        return std::make_unique<XsMomentum>(lookback, top_k);
    }
    // Single-symbol strategies run across the basket, one instance per symbol.
    // Validate params eagerly (this throws on bad input) before the run starts,
    // then hand the factory to the basket.
    make_single_symbol_strategy(id, params);
    return std::make_unique<BasketStrategy>(
        [id, params] { return make_single_symbol_strategy(id, params); });
}

json strategy_catalog() {
    auto int_param = [](const char* name, const char* label, int def, int min, int max) {
        return json{{"name", name}, {"label", label}, {"type", "int"},
                    {"default", def}, {"min", min}, {"max", max}};
    };
    auto num_param = [](const char* name, const char* label, double def, double min, double max) {
        return json{{"name", name}, {"label", label}, {"type", "number"},
                    {"default", def}, {"min", min}, {"max", max}};
    };

    return json::array({
        {{"id", "sma_crossover"},
         {"name", "SMA Crossover"},
         {"description", "Long when the fast simple moving average is above the slow one; flat otherwise."},
         {"params", json::array({int_param("fast_period", "Fast period", 10, 2, 500),
                                 int_param("slow_period", "Slow period", 50, 3, 1000)})}},
        {{"id", "ema_momentum"},
         {"name", "EMA Momentum"},
         {"description", "Trend-following: long while price closes above its exponential moving average."},
         {"params", json::array({int_param("period", "EMA period", 20, 2, 1000)})}},
        {{"id", "bollinger_meanrev"},
         {"name", "Bollinger Mean Reversion"},
         {"description", "Buy when price closes below the lower Bollinger band; exit at the middle band."},
         {"params", json::array({int_param("period", "Band period", 20, 5, 500),
                                 num_param("num_std", "Std deviations", 2.0, 0.5, 5.0)})}},
        {{"id", "rsi_reversion"},
         {"name", "RSI Reversion"},
         {"description", "Buy when RSI is oversold; exit when it becomes overbought."},
         {"params", json::array({int_param("period", "RSI period", 14, 2, 200),
                                 num_param("oversold", "Oversold level", 30.0, 1.0, 99.0),
                                 num_param("overbought", "Overbought level", 70.0, 1.0, 99.0)})}},
        {{"id", "buy_hold"},
         {"name", "Buy & Hold"},
         {"description", "Buy on the first bar and hold. Benchmark strategy."},
         {"params", json::array()}},
        {{"id", "xs_momentum"},
         {"name", "Cross-Sectional Momentum"},
         {"description", "Multi-symbol: each bar, rank the universe by trailing return and hold the top-k "
                         "names with positive momentum, equal-weight. Needs more than one symbol."},
         {"params", json::array({int_param("lookback", "Lookback (bars)", 60, 2, 500),
                                 int_param("top_k", "Number to hold", 3, 1, 50)})}},
    });
}

}  // namespace bt
