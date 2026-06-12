package com.samueltan.backtest.backtest.dto;

import com.fasterxml.jackson.databind.JsonNode;
import com.samueltan.backtest.backtest.JobStatus;
import java.time.Instant;
import java.time.LocalDate;
import java.util.UUID;

public final class JobDtos {

    private JobDtos() {
    }

    /** Compact row for job lists and comparisons. */
    public record JobSummary(
            UUID id,
            String strategyId,
            JsonNode params,
            String symbol,
            LocalDate startDate,
            LocalDate endDate,
            JobStatus status,
            String errorMessage,
            Instant createdAt,
            MetricsSummary metrics) {
    }

    public record MetricsSummary(
            double totalReturn,
            double cagr,
            double sharpe,
            double sortino,
            double maxDrawdown,
            int maxDrawdownDays,
            double winRate,
            double profitFactor,
            int numTrades,
            double finalEquity) {
    }

    /** Full job detail; {@code results} is present only when COMPLETED. */
    public record JobDetail(
            UUID id,
            String strategyId,
            JsonNode params,
            String symbol,
            LocalDate startDate,
            LocalDate endDate,
            double initialCapital,
            double commissionBps,
            double slippageBps,
            double positionFraction,
            JobStatus status,
            String errorMessage,
            Instant createdAt,
            Instant startedAt,
            Instant finishedAt,
            JsonNode results) {
    }
}
