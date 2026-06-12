#include <catch2/catch_amalgamated.hpp>

#include "bt/metrics.hpp"

using namespace bt;
using Catch::Approx;

TEST_CASE("days_between handles month and leap-year boundaries") {
    REQUIRE(days_between("2024-01-01", "2024-01-05") == 4);
    REQUIRE(days_between("2024-02-28", "2024-03-01") == 2);   // 2024 is a leap year
    REQUIRE(days_between("2023-02-28", "2023-03-01") == 1);
    REQUIRE(days_between("2015-01-01", "2025-01-01") == 3653);
}

TEST_CASE("metrics from a hand-computed equity curve and trade log") {
    std::vector<EquityPoint> curve = {
        {"2024-01-01", 100.0}, {"2024-01-02", 110.0}, {"2024-01-03", 99.0},
        {"2024-01-04", 104.5}, {"2024-01-05", 121.0},
    };
    std::vector<Trade> trades = {
        {"a", "b", 1, 0, 0, 10.0, 0.1},
        {"a", "b", 1, 0, 0, -5.0, -0.05},
        {"a", "b", 1, 0, 0, 20.0, 0.2},
        {"a", "b", 1, 0, 0, -10.0, -0.1},
    };

    Metrics m = compute_metrics(curve, trades, 100.0);

    REQUIRE(m.total_return == Approx(0.21));
    REQUIRE(m.final_equity == Approx(121.0));
    // Peak 110 on 01-02, trough 99 on 01-03 -> 10% drawdown.
    REQUIRE(m.max_drawdown == Approx(0.1));
    // Underwater from the 01-02 peak until the new high on 01-05: 3 days.
    REQUIRE(m.max_drawdown_days == 3);
    REQUIRE(m.num_trades == 4);
    REQUIRE(m.win_rate == Approx(0.5));
    REQUIRE(m.profit_factor == Approx(30.0 / 15.0));
    // CAGR: (1.21)^(365.25/4) - 1 over 4 calendar days.
    REQUIRE(m.cagr == Approx(std::pow(1.21, 365.25 / 4.0) - 1.0).epsilon(1e-9));
    REQUIRE(m.sharpe != 0.0);
}

TEST_CASE("flat equity curve produces zero-risk metrics without dividing by zero") {
    std::vector<EquityPoint> curve = {
        {"2024-01-01", 100.0}, {"2024-01-02", 100.0}, {"2024-01-03", 100.0}};
    Metrics m = compute_metrics(curve, {}, 100.0);
    REQUIRE(m.total_return == Approx(0.0));
    REQUIRE(m.max_drawdown == Approx(0.0));
    REQUIRE(m.sharpe == Approx(0.0));
    REQUIRE(m.win_rate == Approx(0.0));
    REQUIRE(m.num_trades == 0);
}
