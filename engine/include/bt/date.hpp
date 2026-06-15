#pragma once

#include <charconv>
#include <chrono>
#include <compare>
#include <cstdio>
#include <stdexcept>
#include <string>
#include <string_view>

namespace bt {

// A calendar date (no time of day), stored as a count of days via
// std::chrono::sys_days so that comparison and day arithmetic are correct by
// construction — leap years and month lengths are handled by <chrono>, not by
// hand. Dates are parsed from / formatted to ISO "YYYY-MM-DD" only at the
// CSV-input and JSON-output boundaries; internally the engine passes Date around
// rather than raw strings.
class Date {
public:
    Date() = default;
    explicit constexpr Date(std::chrono::sys_days days) noexcept : days_(days) {}

    // Parse ISO "YYYY-MM-DD". Throws std::invalid_argument on malformed input.
    static Date parse(std::string_view s) {
        auto fail = [&] {
            throw std::invalid_argument("date must be YYYY-MM-DD, got: " + std::string(s));
        };
        if (s.size() != 10 || s[4] != '-' || s[7] != '-') fail();

        auto to_int = [](std::string_view sv, int& out) {
            const char* end = sv.data() + sv.size();
            auto [ptr, ec] = std::from_chars(sv.data(), end, out);
            return ec == std::errc{} && ptr == end;
        };
        int y = 0, mo = 0, d = 0;
        if (!to_int(s.substr(0, 4), y) || !to_int(s.substr(5, 2), mo) || !to_int(s.substr(8, 2), d))
            fail();

        std::chrono::year_month_day ymd{std::chrono::year{y},
                                        std::chrono::month{static_cast<unsigned>(mo)},
                                        std::chrono::day{static_cast<unsigned>(d)}};
        if (!ymd.ok()) fail();
        return Date{std::chrono::sys_days{ymd}};
    }

    // Format as ISO "YYYY-MM-DD".
    std::string to_string() const {
        std::chrono::year_month_day ymd{days_};
        char buf[16];
        std::snprintf(buf, sizeof buf, "%04d-%02u-%02u",
                      static_cast<int>(ymd.year()),
                      static_cast<unsigned>(ymd.month()),
                      static_cast<unsigned>(ymd.day()));
        return std::string(buf);
    }

    // Calendar days from *this to `other` (other - *this); negative if `other`
    // is earlier.
    int days_until(Date other) const noexcept {
        return static_cast<int>((other.days_ - days_).count());
    }

    // Ordered and equality-comparable; usable as a std::map/std::set key.
    // Compared on the underlying day count (libc++ does not supply a three-way
    // comparison for time_point itself).
    bool operator==(const Date& other) const noexcept {
        return days_.time_since_epoch().count() == other.days_.time_since_epoch().count();
    }
    std::strong_ordering operator<=>(const Date& other) const noexcept {
        return days_.time_since_epoch().count() <=> other.days_.time_since_epoch().count();
    }

private:
    std::chrono::sys_days days_{};  // value-initializes to the epoch (1970-01-01)
};

}  // namespace bt
