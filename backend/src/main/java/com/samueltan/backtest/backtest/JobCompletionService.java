package com.samueltan.backtest.backtest;

import com.fasterxml.jackson.databind.JsonNode;
import org.springframework.stereotype.Service;
import org.springframework.transaction.annotation.Transactional;

/**
 * Persists a job's terminal outcome. Splitting this out of {@link BacktestWorker}
 * gives the completion path a real transaction boundary (a {@code @Transactional}
 * method called on the worker itself would be bypassed by self-invocation), so
 * the result row and the COMPLETED status are written atomically — a crash can
 * no longer leave a saved result attached to a non-completed job.
 */
@Service
public class JobCompletionService {

    private final BacktestJobRepository jobs;
    private final BacktestResultRepository results;

    public JobCompletionService(BacktestJobRepository jobs, BacktestResultRepository results) {
        this.jobs = jobs;
        this.results = results;
    }

    @Transactional
    public void complete(BacktestJob job, JsonNode resultsNode) {
        JsonNode m = resultsNode.get("metrics");
        results.save(new BacktestResult(
                job.getId(),
                m.get("total_return").asDouble(),
                m.get("cagr").asDouble(),
                m.get("sharpe").asDouble(),
                m.get("sortino").asDouble(),
                m.get("max_drawdown").asDouble(),
                m.get("max_drawdown_days").asInt(),
                m.get("win_rate").asDouble(),
                m.get("profit_factor").asDouble(),
                m.get("num_trades").asInt(),
                m.get("final_equity").asDouble(),
                resultsNode.toString()));
        job.markCompleted();
        jobs.save(job);
    }

    @Transactional
    public void fail(BacktestJob job, String message) {
        job.markFailed(message);
        jobs.save(job);
    }
}
