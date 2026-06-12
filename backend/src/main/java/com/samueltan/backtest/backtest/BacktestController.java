package com.samueltan.backtest.backtest;

import com.samueltan.backtest.backtest.dto.BacktestRequest;
import com.samueltan.backtest.backtest.dto.JobDtos.JobDetail;
import com.samueltan.backtest.backtest.dto.JobDtos.JobSummary;
import jakarta.validation.Valid;
import java.util.UUID;
import org.springframework.data.domain.Page;
import org.springframework.data.domain.PageRequest;
import org.springframework.http.HttpStatus;
import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.DeleteMapping;
import org.springframework.web.bind.annotation.GetMapping;
import org.springframework.web.bind.annotation.PathVariable;
import org.springframework.web.bind.annotation.PostMapping;
import org.springframework.web.bind.annotation.RequestBody;
import org.springframework.web.bind.annotation.RequestMapping;
import org.springframework.web.bind.annotation.RequestParam;
import org.springframework.web.bind.annotation.RestController;

@RestController
@RequestMapping("/api/backtests")
public class BacktestController {

    private final BacktestService service;

    public BacktestController(BacktestService service) {
        this.service = service;
    }

    /** Long-running work: accept, return the job, let the client poll. */
    @PostMapping
    public ResponseEntity<JobDetail> submit(@Valid @RequestBody BacktestRequest request) {
        JobDetail job = service.submit(request);
        return ResponseEntity.status(HttpStatus.ACCEPTED).body(job);
    }

    @GetMapping("/{id}")
    public JobDetail get(@PathVariable UUID id) {
        return service.get(id);
    }

    @GetMapping
    public Page<JobSummary> list(@RequestParam(defaultValue = "0") int page,
                                 @RequestParam(defaultValue = "20") int size) {
        return service.list(PageRequest.of(page, Math.min(size, 100)));
    }

    @DeleteMapping("/{id}")
    public ResponseEntity<Void> delete(@PathVariable UUID id) {
        service.delete(id);
        return ResponseEntity.noContent().build();
    }
}
