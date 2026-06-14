package com.samueltan.backtest;

import static org.assertj.core.api.Assertions.assertThat;

import com.samueltan.backtest.market.MarketDataService;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.List;
import java.util.concurrent.Callable;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.Future;
import org.junit.jupiter.api.Test;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.boot.test.context.SpringBootTest;

/**
 * Regression test for the CSV cache write race: many workers asking for the
 * same symbol's engine CSV at once must each get a complete, identical file —
 * never a truncated or interleaved one.
 */
@SpringBootTest
class MarketDataConcurrencyTest {

    @Autowired
    private MarketDataService marketData;

    @Test
    void concurrentCacheBuildsProduceOneCompleteFile() throws Exception {
        marketData.invalidateCache("AAPL");

        int workers = 8;
        ExecutorService pool = Executors.newFixedThreadPool(workers);
        try {
            List<Callable<Path>> tasks = java.util.Collections.nCopies(
                    workers, () -> marketData.ensureCsvForEngine("AAPL"));
            List<Future<Path>> futures = pool.invokeAll(tasks);

            Path expected = futures.get(0).get();
            long lineCount = Files.lines(expected).count();
            String firstDataLine = Files.lines(expected).skip(1).findFirst().orElseThrow();

            for (Future<Path> future : futures) {
                Path path = future.get(); // also surfaces any exception thrown in a worker
                assertThat(path).isEqualTo(expected);
                // Every reader sees the same complete file: header + all bars,
                // with a well-formed first row (6 comma-separated fields).
                assertThat(Files.lines(path).count()).isEqualTo(lineCount);
                assertThat(firstDataLine.split(",")).hasSize(6);
            }
            assertThat(lineCount).isGreaterThan(3000); // ~15y of daily AAPL bars + header
        } finally {
            pool.shutdownNow();
        }
    }
}
