#include <catch2/catch_amalgamated.hpp>

#include "bt/indicators.hpp"

using namespace bt;
using Catch::Approx;

TEST_CASE("SMA computes rolling mean over fixed window") {
    Sma sma(3);
    sma.update(10.0);
    sma.update(20.0);
    REQUIRE_FALSE(sma.ready());
    sma.update(30.0);
    REQUIRE(sma.ready());
    REQUIRE(sma.value() == Approx(20.0));
    sma.update(40.0);  // window is now 20,30,40
    REQUIRE(sma.value() == Approx(30.0));
}

TEST_CASE("EMA seeds with SMA then smooths exponentially") {
    Ema ema(3);  // alpha = 0.5
    ema.update(10.0);
    ema.update(20.0);
    REQUIRE_FALSE(ema.ready());
    ema.update(30.0);
    REQUIRE(ema.ready());
    REQUIRE(ema.value() == Approx(20.0));  // seed = mean(10,20,30)
    ema.update(40.0);
    REQUIRE(ema.value() == Approx(0.5 * 40.0 + 0.5 * 20.0));  // 30
}

TEST_CASE("Rolling stddev over fixed window") {
    RollingStdDev sd(4);
    for (double v : {2.0, 4.0, 4.0, 4.0}) sd.update(v);
    REQUIRE(sd.ready());
    // mean 3.5, population var = (2.25 + 0.25*3)/4 = 0.75
    REQUIRE(sd.value() == Approx(std::sqrt(0.75)));
    sd.update(6.0);  // window 4,4,4,6: mean 4.5, var = (0.25*3 + 2.25)/4
    REQUIRE(sd.value() == Approx(std::sqrt(0.75)));
}

TEST_CASE("RSI is 100 for monotonic gains and ~0 for monotonic losses") {
    Rsi up(3);
    for (double v : {10.0, 11.0, 12.0, 13.0, 14.0}) up.update(v);
    REQUIRE(up.ready());
    REQUIRE(up.value() == Approx(100.0));

    Rsi down(3);
    for (double v : {14.0, 13.0, 12.0, 11.0, 10.0}) down.update(v);
    REQUIRE(down.ready());
    REQUIRE(down.value() == Approx(0.0).margin(1e-9));
}

TEST_CASE("RSI of alternating equal gains and losses is 50") {
    Rsi rsi(4);
    for (double v : {10.0, 11.0, 10.0, 11.0, 10.0, 11.0, 10.0, 11.0, 10.0}) rsi.update(v);
    REQUIRE(rsi.ready());
    REQUIRE(rsi.value() == Approx(50.0).margin(5.0));
}
