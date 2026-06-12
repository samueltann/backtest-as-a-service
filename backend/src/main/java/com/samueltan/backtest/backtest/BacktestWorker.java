package com.samueltan.backtest.backtest;

import com.fasterxml.jackson.databind.JsonNode;
import com.fasterxml.jackson.databind.ObjectMapper;
import com.fasterxml.jackson.databind.node.ObjectNode;
import com.samueltan.backtest.engine.EngineRunner;
import com.samueltan.backtest.market.MarketDataService;
import java.nio.file.Path;
import java.util.UUID;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.stereotype.Component;

/**
 * Executes one queued job end to end: exports the data the engine needs,
 * builds its config, runs the process, and persists the outcome. Runs on the
 * backtest executor pool, never on a request thread.
 */
@Component
public class BacktestWorker {

    private static final Logger log = LoggerFactory.getLogger(BacktestWorker.class);

    private final BacktestJobRepository jobs;
    private final BacktestResultRepository results;
    private final MarketDataService marketData;
    private final EngineRunner engineRunner;
    private final ObjectMapper mapper;

    public BacktestWorker(BacktestJobRepository jobs, BacktestResultRepository results,
                          MarketDataService marketData, EngineRunner engineRunner,
                          ObjectMapper mapper) {
        this.jobs = jobs;
        this.results = results;
        this.marketData = marketData;
        this.engineRunner = engineRunner;
        this.mapper = mapper;
    }

    public void execute(UUID jobId) {
        BacktestJob job = jobs.findById(jobId).orElse(null);
        if (job == null) {
            return; // deleted while queued
        }
        try {
            job.markRunning();
            jobs.saveAndFlush(job);

            Path dataFile = marketData.ensureCsvForEngine(job.getSymbol());
            EngineRunner.EngineResult outcome =
                    engineRunner.run(job.getId().toString(), buildEngineConfig(job, dataFile));

            if (outcome.isSuccess()) {
                saveResult(job, outcome.results());
                job.markCompleted();
            } else {
                job.markFailed(outcome.errorMessage());
            }
        } catch (Exception e) {
            log.error("Backtest {} failed unexpectedly", jobId, e);
            job.markFailed("internal error: " + e.getMessage());
        }
        jobs.saveAndFlush(job);
    }

    private JsonNode buildEngineConfig(BacktestJob job, Path dataFile) throws Exception {
        ObjectNode strategy = mapper.createObjectNode();
        strategy.put("id", job.getStrategyId());
        strategy.set("params", mapper.readTree(job.getParamsJson()));

        ObjectNode config = mapper.createObjectNode();
        config.set("strategy", strategy);
        config.put("symbol", job.getSymbol());
        config.put("data_file", dataFile.toAbsolutePath().toString());
        config.put("start_date", job.getStartDate().toString());
        config.put("end_date", job.getEndDate().toString());
        config.put("initial_capital", job.getInitialCapital());
        config.put("commission_bps", job.getCommissionBps());
        config.put("slippage_bps", job.getSlippageBps());
        config.put("position_fraction", job.getPositionFraction());
        return config;
    }

    private void saveResult(BacktestJob job, JsonNode resultsNode) {
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
    }
}
