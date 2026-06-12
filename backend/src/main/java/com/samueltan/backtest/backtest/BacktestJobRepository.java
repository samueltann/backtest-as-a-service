package com.samueltan.backtest.backtest;

import java.util.List;
import java.util.UUID;
import org.springframework.data.domain.Page;
import org.springframework.data.domain.Pageable;
import org.springframework.data.jpa.repository.JpaRepository;

public interface BacktestJobRepository extends JpaRepository<BacktestJob, UUID> {

    Page<BacktestJob> findAllByOrderByCreatedAtDesc(Pageable pageable);

    List<BacktestJob> findByStatusIn(List<JobStatus> statuses);
}
