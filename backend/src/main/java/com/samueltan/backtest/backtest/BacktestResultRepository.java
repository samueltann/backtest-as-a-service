package com.samueltan.backtest.backtest;

import java.util.List;
import java.util.Optional;
import java.util.UUID;
import org.springframework.data.jpa.repository.JpaRepository;

public interface BacktestResultRepository extends JpaRepository<BacktestResult, Long> {

    Optional<BacktestResult> findByJobId(UUID jobId);

    List<BacktestResult> findByJobIdIn(List<UUID> jobIds);

    void deleteByJobId(UUID jobId);
}
