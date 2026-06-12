package com.samueltan.backtest.backtest;

import com.fasterxml.jackson.core.JsonProcessingException;
import com.fasterxml.jackson.databind.JsonNode;
import com.fasterxml.jackson.databind.ObjectMapper;
import com.samueltan.backtest.backtest.dto.BacktestRequest;
import com.samueltan.backtest.backtest.dto.JobDtos.JobDetail;
import com.samueltan.backtest.backtest.dto.JobDtos.JobSummary;
import com.samueltan.backtest.backtest.dto.JobDtos.MetricsSummary;
import com.samueltan.backtest.market.MarketDataService;
import com.samueltan.backtest.strategy.StrategyCatalogService;
import jakarta.annotation.PostConstruct;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.UUID;
import java.util.concurrent.RejectedExecutionException;
import java.util.function.Function;
import java.util.stream.Collectors;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.beans.factory.annotation.Qualifier;
import org.springframework.core.task.TaskExecutor;
import org.springframework.data.domain.Page;
import org.springframework.data.domain.Pageable;
import org.springframework.stereotype.Service;
import org.springframework.transaction.annotation.Transactional;

/**
 * Validates and persists backtest jobs, hands them to the worker pool, and
 * assembles API views. Submission returns immediately (202 pattern): clients
 * poll job status until it reaches COMPLETED or FAILED.
 */
@Service
public class BacktestService {

    private static final Logger log = LoggerFactory.getLogger(BacktestService.class);

    private final BacktestJobRepository jobs;
    private final BacktestResultRepository results;
    private final StrategyCatalogService catalog;
    private final MarketDataService marketData;
    private final BacktestWorker worker;
    private final TaskExecutor executor;
    private final ObjectMapper mapper;

    public BacktestService(BacktestJobRepository jobs, BacktestResultRepository results,
                           StrategyCatalogService catalog, MarketDataService marketData,
                           BacktestWorker worker,
                           @Qualifier("backtestExecutor") TaskExecutor executor,
                           ObjectMapper mapper) {
        this.jobs = jobs;
        this.results = results;
        this.catalog = catalog;
        this.marketData = marketData;
        this.worker = worker;
        this.executor = executor;
        this.mapper = mapper;
    }

    /** Jobs that were queued or mid-run when the server died can never finish. */
    @PostConstruct
    void failOrphanedJobs() {
        List<BacktestJob> orphans = jobs.findByStatusIn(List.of(JobStatus.QUEUED, JobStatus.RUNNING));
        for (BacktestJob job : orphans) {
            job.markFailed("server restarted before the job finished");
        }
        if (!orphans.isEmpty()) {
            jobs.saveAll(orphans);
            log.info("Marked {} orphaned job(s) as FAILED", orphans.size());
        }
    }

    public JobDetail submit(BacktestRequest request, Long userId) {
        String symbol = request.symbol().toUpperCase(Locale.ROOT);
        catalog.validateParams(request.strategyId(), request.params());
        marketData.bars(symbol, request.startDate(), request.endDate()); // throws if unknown

        BacktestJob job = new BacktestJob(
                UUID.randomUUID(),
                request.strategyId(),
                writeJson(request.params()),
                symbol,
                request.startDate(),
                request.endDate(),
                request.initialCapital(),
                request.commissionBps(),
                request.slippageBps(),
                request.positionFraction());
        job.setUserId(userId);
        jobs.save(job);

        UUID jobId = job.getId();
        try {
            executor.execute(() -> worker.execute(jobId));
        } catch (RejectedExecutionException e) {
            job.markFailed("server is at capacity, try again later");
            jobs.save(job);
            throw new CapacityExceededException();
        }
        return toDetail(job, null);
    }

    /** Jobs are scoped to their owner: another user's id behaves like a 404. */
    @Transactional(readOnly = true)
    public JobDetail get(UUID id, Long userId) {
        BacktestJob job = jobs.findByIdAndUserId(id, userId)
                .orElseThrow(() -> new JobNotFoundException(id));
        JsonNode resultsNode = results.findByJobId(id)
                .map(r -> readJson(r.getResultsJson()))
                .orElse(null);
        return toDetail(job, resultsNode);
    }

    @Transactional(readOnly = true)
    public Page<JobSummary> list(Pageable pageable, Long userId) {
        Page<BacktestJob> page = jobs.findByUserIdOrderByCreatedAtDesc(userId, pageable);
        List<UUID> ids = page.map(BacktestJob::getId).toList();
        Map<UUID, BacktestResult> byJob = results.findByJobIdIn(ids).stream()
                .collect(Collectors.toMap(BacktestResult::getJobId, Function.identity()));
        return page.map(job -> toSummary(job, byJob.get(job.getId())));
    }

    @Transactional
    public void delete(UUID id, Long userId) {
        BacktestJob job = jobs.findByIdAndUserId(id, userId)
                .orElseThrow(() -> new JobNotFoundException(id));
        results.deleteByJobId(id);
        jobs.delete(job);
    }

    private JobSummary toSummary(BacktestJob job, BacktestResult result) {
        MetricsSummary metrics = result == null ? null : new MetricsSummary(
                result.getTotalReturn(), result.getCagr(), result.getSharpe(),
                result.getSortino(), result.getMaxDrawdown(), result.getMaxDrawdownDays(),
                result.getWinRate(), result.getProfitFactor(), result.getNumTrades(),
                result.getFinalEquity());
        return new JobSummary(job.getId(), job.getStrategyId(), readJson(job.getParamsJson()),
                job.getSymbol(), job.getStartDate(), job.getEndDate(), job.getStatus(),
                job.getErrorMessage(), job.getCreatedAt(), metrics);
    }

    private JobDetail toDetail(BacktestJob job, JsonNode resultsNode) {
        return new JobDetail(job.getId(), job.getStrategyId(), readJson(job.getParamsJson()),
                job.getSymbol(), job.getStartDate(), job.getEndDate(), job.getInitialCapital(),
                job.getCommissionBps(), job.getSlippageBps(), job.getPositionFraction(),
                job.getStatus(), job.getErrorMessage(), job.getCreatedAt(), job.getStartedAt(),
                job.getFinishedAt(), resultsNode);
    }

    private String writeJson(Object value) {
        try {
            return mapper.writeValueAsString(value);
        } catch (JsonProcessingException e) {
            throw new IllegalArgumentException("unserializable params", e);
        }
    }

    private JsonNode readJson(String json) {
        try {
            return mapper.readTree(json);
        } catch (JsonProcessingException e) {
            throw new IllegalStateException("corrupt stored JSON", e);
        }
    }

    public static class JobNotFoundException extends RuntimeException {
        public JobNotFoundException(UUID id) {
            super("job not found: " + id);
        }
    }

    public static class CapacityExceededException extends RuntimeException {
        public CapacityExceededException() {
            super("server is at capacity, try again later");
        }
    }
}
