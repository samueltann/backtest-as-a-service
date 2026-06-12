#pragma once

#include <cmath>
#include <cstddef>
#include <deque>

namespace bt {

// Rolling simple moving average over a fixed window. O(1) per update.
class Sma {
public:
    explicit Sma(std::size_t period) : period_(period) {}

    void update(double value) {
        window_.push_back(value);
        sum_ += value;
        if (window_.size() > period_) {
            sum_ -= window_.front();
            window_.pop_front();
        }
    }

    bool ready() const { return window_.size() == period_; }
    double value() const { return sum_ / static_cast<double>(window_.size()); }

private:
    std::size_t period_;
    std::deque<double> window_;
    double sum_ = 0.0;
};

// Exponential moving average, seeded with the SMA of the first `period` values.
class Ema {
public:
    explicit Ema(std::size_t period)
        : period_(period), alpha_(2.0 / (static_cast<double>(period) + 1.0)) {}

    void update(double value) {
        ++count_;
        if (count_ <= period_) {
            seed_sum_ += value;
            value_ = seed_sum_ / static_cast<double>(count_);
        } else {
            value_ = alpha_ * value + (1.0 - alpha_) * value_;
        }
    }

    bool ready() const { return count_ >= period_; }
    double value() const { return value_; }

private:
    std::size_t period_;
    double alpha_;
    std::size_t count_ = 0;
    double seed_sum_ = 0.0;
    double value_ = 0.0;
};

// Rolling standard deviation (population) over a fixed window.
class RollingStdDev {
public:
    explicit RollingStdDev(std::size_t period) : period_(period) {}

    void update(double value) {
        window_.push_back(value);
        sum_ += value;
        sum_sq_ += value * value;
        if (window_.size() > period_) {
            double old = window_.front();
            window_.pop_front();
            sum_ -= old;
            sum_sq_ -= old * old;
        }
    }

    bool ready() const { return window_.size() == period_; }

    double value() const {
        double n = static_cast<double>(window_.size());
        double mean = sum_ / n;
        double var = sum_sq_ / n - mean * mean;
        return var > 0.0 ? std::sqrt(var) : 0.0;
    }

private:
    std::size_t period_;
    std::deque<double> window_;
    double sum_ = 0.0;
    double sum_sq_ = 0.0;
};

// Wilder's RSI: smoothed average gains vs losses, range 0-100.
class Rsi {
public:
    explicit Rsi(std::size_t period) : period_(period) {}

    void update(double close) {
        if (count_ == 0) {
            prev_close_ = close;
            ++count_;
            return;
        }
        double change = close - prev_close_;
        double gain = change > 0 ? change : 0.0;
        double loss = change < 0 ? -change : 0.0;
        if (count_ <= period_) {
            // Build the initial simple averages over the first `period` changes.
            avg_gain_ += (gain - avg_gain_) / static_cast<double>(count_);
            avg_loss_ += (loss - avg_loss_) / static_cast<double>(count_);
        } else {
            double p = static_cast<double>(period_);
            avg_gain_ = (avg_gain_ * (p - 1.0) + gain) / p;
            avg_loss_ = (avg_loss_ * (p - 1.0) + loss) / p;
        }
        prev_close_ = close;
        ++count_;
    }

    bool ready() const { return count_ > period_; }

    double value() const {
        if (avg_loss_ == 0.0) return 100.0;
        double rs = avg_gain_ / avg_loss_;
        return 100.0 - 100.0 / (1.0 + rs);
    }

private:
    std::size_t period_;
    std::size_t count_ = 0;
    double prev_close_ = 0.0;
    double avg_gain_ = 0.0;
    double avg_loss_ = 0.0;
};

}  // namespace bt
