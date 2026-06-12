package com.samueltan.backtest.backtest.dto;

import jakarta.validation.constraints.AssertTrue;
import jakarta.validation.constraints.DecimalMax;
import jakarta.validation.constraints.NotBlank;
import jakarta.validation.constraints.NotNull;
import jakarta.validation.constraints.Positive;
import jakarta.validation.constraints.PositiveOrZero;
import java.time.LocalDate;
import java.util.Map;

public record BacktestRequest(
        @NotBlank String strategyId,
        Map<String, Object> params,
        @NotBlank String symbol,
        @NotNull LocalDate startDate,
        @NotNull LocalDate endDate,
        @NotNull @Positive Double initialCapital,
        @PositiveOrZero double commissionBps,
        @PositiveOrZero double slippageBps,
        @Positive @DecimalMax("1.0") double positionFraction) {

    public BacktestRequest {
        if (params == null) {
            params = Map.of();
        }
        if (positionFraction == 0.0) {
            positionFraction = 0.95;
        }
    }

    @AssertTrue(message = "startDate must be before endDate")
    public boolean isDateRangeValid() {
        return startDate == null || endDate == null || startDate.isBefore(endDate);
    }
}
