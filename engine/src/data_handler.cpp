#include "bt/data_handler.hpp"

#include <algorithm>
#include <fstream>
#include <set>
#include <sstream>

namespace bt {

namespace {

std::vector<std::string> split_csv_line(const std::string& line) {
    std::vector<std::string> out;
    std::stringstream ss(line);
    std::string field;
    while (std::getline(ss, field, ',')) out.push_back(field);
    return out;
}

}  // namespace

std::vector<Bar> load_csv(const std::string& csv_path,
                          const std::string& symbol,
                          const std::string& start_date,
                          const std::string& end_date) {
    std::ifstream in(csv_path);
    if (!in) throw EngineError("cannot open data file: " + csv_path);

    std::string line;
    if (!std::getline(in, line)) throw EngineError("empty data file: " + csv_path);

    // Header: locate columns by name so column order doesn't matter.
    auto header = split_csv_line(line);
    int idx_date = -1, idx_open = -1, idx_high = -1, idx_low = -1, idx_close = -1, idx_vol = -1;
    for (int i = 0; i < static_cast<int>(header.size()); ++i) {
        std::string h = header[i];
        std::transform(h.begin(), h.end(), h.begin(), ::tolower);
        if (!h.empty() && h.back() == '\r') h.pop_back();
        if (h == "date") idx_date = i;
        else if (h == "open") idx_open = i;
        else if (h == "high") idx_high = i;
        else if (h == "low") idx_low = i;
        else if (h == "close") idx_close = i;
        else if (h == "volume") idx_vol = i;
    }
    if (idx_date < 0 || idx_open < 0 || idx_high < 0 || idx_low < 0 || idx_close < 0)
        throw EngineError("data file missing required columns (Date,Open,High,Low,Close): " + csv_path);

    std::vector<Bar> bars;
    int line_no = 1;
    while (std::getline(in, line)) {
        ++line_no;
        if (line.empty() || line == "\r") continue;
        auto fields = split_csv_line(line);
        if (static_cast<int>(fields.size()) <= std::max({idx_date, idx_open, idx_high, idx_low, idx_close}))
            throw EngineError("malformed CSV row at line " + std::to_string(line_no));

        Bar bar;
        bar.symbol = symbol;
        bar.date = fields[idx_date];
        if (bar.date < start_date || bar.date > end_date) continue;
        try {
            bar.open = std::stod(fields[idx_open]);
            bar.high = std::stod(fields[idx_high]);
            bar.low = std::stod(fields[idx_low]);
            bar.close = std::stod(fields[idx_close]);
            bar.volume = (idx_vol >= 0 && idx_vol < static_cast<int>(fields.size()))
                             ? std::stod(fields[idx_vol]) : 0.0;
        } catch (const std::exception&) {
            throw EngineError("non-numeric value in CSV at line " + std::to_string(line_no));
        }
        bars.push_back(std::move(bar));
    }

    std::sort(bars.begin(), bars.end(),
              [](const Bar& a, const Bar& b) { return a.date < b.date; });

    if (bars.empty())
        throw EngineError("no bars in range " + start_date + " to " + end_date +
                          (symbol.empty() ? "" : " for " + symbol));
    return bars;
}

MultiCsvDataHandler::MultiCsvDataHandler(
    const std::vector<std::pair<std::string, std::string>>& symbol_files,
    const std::string& start_date,
    const std::string& end_date) {
    if (symbol_files.empty()) throw EngineError("no symbols provided");

    std::set<std::string> all_dates;
    for (const auto& [symbol, path] : symbol_files) {
        if (series_.count(symbol)) throw EngineError("duplicate symbol: " + symbol);
        auto bars = load_csv(path, symbol, start_date, end_date);
        for (const auto& bar : bars) all_dates.insert(bar.date);
        cursor_[symbol] = 0;
        series_.emplace(symbol, std::move(bars));
    }
    dates_.assign(all_dates.begin(), all_dates.end());  // std::set is already sorted
}

std::optional<TimeSlice> MultiCsvDataHandler::next_slice() {
    if (date_cursor_ >= dates_.size()) return std::nullopt;
    const std::string& date = dates_[date_cursor_++];

    TimeSlice slice;
    slice.date = date;
    // Every series is sorted ascending and `dates_` is their sorted union, so a
    // symbol's cursor either points at this date (include it and advance) or a
    // later one (skip it for now).
    for (auto& [symbol, bars] : series_) {
        std::size_t& cur = cursor_[symbol];
        if (cur < bars.size() && bars[cur].date == date) {
            slice.bars.emplace(symbol, bars[cur]);
            ++cur;
        }
    }
    return slice;
}

CsvDataHandler::CsvDataHandler(const std::string& csv_path,
                               const std::string& start_date,
                               const std::string& end_date)
    : bars_(load_csv(csv_path, /*symbol=*/"", start_date, end_date)) {}

std::optional<Bar> CsvDataHandler::next() {
    if (cursor_ >= bars_.size()) return std::nullopt;
    return bars_[cursor_++];
}

}  // namespace bt
