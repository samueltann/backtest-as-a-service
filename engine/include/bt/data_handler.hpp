#pragma once

#include <cstddef>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "bt/events.hpp"

namespace bt {

struct EngineError : std::runtime_error {
    using std::runtime_error::runtime_error;
};

// Loads one symbol's bars from a CSV (Date,Open,High,Low,Close,Volume), filtered
// to [start_date, end_date], sorted chronologically, with bar.symbol stamped.
// Throws EngineError on I/O errors, malformed rows, or an empty range.
std::vector<Bar> load_csv(const std::string& csv_path,
                          const std::string& symbol,
                          const std::string& start_date,
                          const std::string& end_date);

// Streams time slices chronologically across a universe of symbols. Each step
// emits every symbol's bar that shares the next date (the ascending union of all
// symbols' dates). Strategies receive slices one at a time and can never see past
// the current date — the engine's structural guard against lookahead bias.
class MultiCsvDataHandler {
public:
    // symbol_files: (symbol, csv_path) pairs. Order is irrelevant; bars are keyed
    // by symbol internally.
    MultiCsvDataHandler(const std::vector<std::pair<std::string, std::string>>& symbol_files,
                        const std::string& start_date,
                        const std::string& end_date);

    // Returns the next slice and advances, or nullopt when all series are exhausted.
    std::optional<TimeSlice> next_slice();

    std::size_t total_dates() const { return dates_.size(); }

private:
    std::map<std::string, std::vector<Bar>> series_;  // symbol -> chronological bars
    std::map<std::string, std::size_t> cursor_;       // symbol -> next index in series_
    std::vector<std::string> dates_;                  // sorted unique union of all dates
    std::size_t date_cursor_ = 0;
};

// Single-symbol convenience retained for backward compatibility and the existing
// direct-construction tests. A thin wrapper over load_csv + linear iteration.
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
