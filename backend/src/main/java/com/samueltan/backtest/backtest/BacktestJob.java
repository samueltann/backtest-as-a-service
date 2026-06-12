package com.samueltan.backtest.backtest;

import jakarta.persistence.Column;
import jakarta.persistence.Entity;
import jakarta.persistence.EnumType;
import jakarta.persistence.Enumerated;
import jakarta.persistence.Id;
import jakarta.persistence.Lob;
import jakarta.persistence.Table;
import java.time.Instant;
import java.time.LocalDate;
import java.util.UUID;

/**
 * One backtest request and its lifecycle: QUEUED -> RUNNING -> COMPLETED/FAILED.
 * The full submitted config is stored so any run is reproducible.
 */
@Entity
@Table(name = "backtest_jobs")
public class BacktestJob {

    @Id
    private UUID id;

    @Column(nullable = false)
    private String strategyId;

    @Lob
    @Column(nullable = false)
    private String paramsJson;

    @Column(nullable = false)
    private String symbol;

    private LocalDate startDate;
    private LocalDate endDate;
    private double initialCapital;
    private double commissionBps;
    private double slippageBps;
    private double positionFraction;

    @Enumerated(EnumType.STRING)
    @Column(nullable = false)
    private JobStatus status;

    @Column(length = 2000)
    private String errorMessage;

    private Instant createdAt;
    private Instant startedAt;
    private Instant finishedAt;

    /** Owner; nullable until auth lands in phase 4. */
    private Long userId;

    protected BacktestJob() {
    }

    public BacktestJob(UUID id, String strategyId, String paramsJson, String symbol,
                       LocalDate startDate, LocalDate endDate, double initialCapital,
                       double commissionBps, double slippageBps, double positionFraction) {
        this.id = id;
        this.strategyId = strategyId;
        this.paramsJson = paramsJson;
        this.symbol = symbol;
        this.startDate = startDate;
        this.endDate = endDate;
        this.initialCapital = initialCapital;
        this.commissionBps = commissionBps;
        this.slippageBps = slippageBps;
        this.positionFraction = positionFraction;
        this.status = JobStatus.QUEUED;
        this.createdAt = Instant.now();
    }

    public void markRunning() {
        this.status = JobStatus.RUNNING;
        this.startedAt = Instant.now();
    }

    public void markCompleted() {
        this.status = JobStatus.COMPLETED;
        this.finishedAt = Instant.now();
    }

    public void markFailed(String message) {
        this.status = JobStatus.FAILED;
        this.errorMessage = message;
        this.finishedAt = Instant.now();
    }

    public UUID getId() { return id; }
    public String getStrategyId() { return strategyId; }
    public String getParamsJson() { return paramsJson; }
    public String getSymbol() { return symbol; }
    public LocalDate getStartDate() { return startDate; }
    public LocalDate getEndDate() { return endDate; }
    public double getInitialCapital() { return initialCapital; }
    public double getCommissionBps() { return commissionBps; }
    public double getSlippageBps() { return slippageBps; }
    public double getPositionFraction() { return positionFraction; }
    public JobStatus getStatus() { return status; }
    public String getErrorMessage() { return errorMessage; }
    public Instant getCreatedAt() { return createdAt; }
    public Instant getStartedAt() { return startedAt; }
    public Instant getFinishedAt() { return finishedAt; }
    public Long getUserId() { return userId; }
    public void setUserId(Long userId) { this.userId = userId; }
}
