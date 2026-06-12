package com.samueltan.backtest.config;

import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Configuration;
import org.springframework.core.task.TaskExecutor;
import org.springframework.scheduling.concurrent.ThreadPoolTaskExecutor;

@Configuration
public class ExecutorConfig {

    /**
     * The backtest worker pool. Bounded on both axes: at most 4 engines run
     * concurrently (each is a separate OS process) and at most 100 jobs wait
     * in the queue. Beyond that, submissions are rejected and surface to the
     * client as 429 — backpressure instead of unbounded memory growth.
     */
    @Bean(name = "backtestExecutor")
    public TaskExecutor backtestExecutor() {
        ThreadPoolTaskExecutor executor = new ThreadPoolTaskExecutor();
        executor.setCorePoolSize(4);
        executor.setMaxPoolSize(4);
        executor.setQueueCapacity(100);
        executor.setThreadNamePrefix("backtest-");
        executor.setWaitForTasksToCompleteOnShutdown(false);
        executor.initialize();
        return executor;
    }
}
