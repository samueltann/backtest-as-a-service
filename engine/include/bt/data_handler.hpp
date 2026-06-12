#pragma once

#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include "bt/events.hpp"

namespace bt {

struct EngineError : std::runtime_error {
    using std::runtime_error::runtime_error;
};

// Streams bars chronologically from a CSV file (Date,Open,High,Low,Close,Volume).
// Strategies receive bars one at a time and can never see past the current
// bar — this is the engine's structural guard against lookahead bias.
class CsvDataHandler {
public:
    CsvDataHandler(const std::string& csv_path,
                   const std::string& start_date,
                   const std::string& end_date);

    // Returns the next bar and advances, or nullopt when exhausted.
    std::optional<Bar> next();

    std::size_t total_bars() const { return bars_.size(); }

private:
    std::vector<Bar> bars_;
    std::size_t cursor_ = 0;
};

}  // namespace bt
