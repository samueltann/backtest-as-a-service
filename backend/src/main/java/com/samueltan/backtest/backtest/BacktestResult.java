package com.samueltan.backtest.backtest;

import jakarta.persistence.Column;
import jakarta.persistence.Entity;
import jakarta.persistence.GeneratedValue;
import jakarta.persistence.GenerationType;
import jakarta.persistence.Id;
import jakarta.persistence.Lob;
import jakarta.persistence.Table;
import java.util.UUID;

/**
 * Key metrics are first-class columns so job lists and comparisons can be
 * served without parsing the (large) results document; the full payload
 * (equity curve + trade log) is kept as JSON text.
 */
@Entity
@Table(name = "backtest_results")
public class BacktestResult {

    @Id
    @GeneratedValue(strategy = GenerationType.IDENTITY)
    private Long id;

    @Column(nullable = false, unique = true)
    private UUID jobId;

    private double totalReturn;
    private double cagr;
    private double sharpe;
    private double sortino;
    private double maxDrawdown;
    private int maxDrawdownDays;
    private double winRate;
    private double profitFactor;
    private int numTrades;
    private double finalEquity;

    @Lob
    @Column(nullable = false)
    private String resultsJson;

    protected BacktestResult() {
    }

    public BacktestResult(UUID jobId, double totalReturn, double cagr, double sharpe,
                          double sortino, double maxDrawdown, int maxDrawdownDays,
                          double winRate, double profitFactor, int numTrades,
                          double finalEquity, String resultsJson) {
        this.jobId = jobId;
        this.totalReturn = totalReturn;
        this.cagr = cagr;
        this.sharpe = sharpe;
        this.sortino = sortino;
        this.maxDrawdown = maxDrawdown;
        this.maxDrawdownDays = maxDrawdownDays;
        this.winRate = winRate;
        this.profitFactor = profitFactor;
        this.numTrades = numTrades;
        this.finalEquity = finalEquity;
        this.resultsJson = resultsJson;
    }

    public Long getId() { return id; }
    public UUID getJobId() { return jobId; }
    public double getTotalReturn() { return totalReturn; }
    public double getCagr() { return cagr; }
    public double getSharpe() { return sharpe; }
    public double getSortino() { return sortino; }
    public double getMaxDrawdown() { return maxDrawdown; }
    public int getMaxDrawdownDays() { return maxDrawdownDays; }
    public double getWinRate() { return winRate; }
    public double getProfitFactor() { return profitFactor; }
    public int getNumTrades() { return numTrades; }
    public double getFinalEquity() { return finalEquity; }
    public String getResultsJson() { return resultsJson; }
}
