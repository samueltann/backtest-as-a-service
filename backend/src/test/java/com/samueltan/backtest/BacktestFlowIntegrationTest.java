package com.samueltan.backtest;

import static org.assertj.core.api.Assertions.assertThat;

import com.fasterxml.jackson.databind.JsonNode;
import java.time.Duration;
import java.time.Instant;
import java.util.Map;
import org.junit.jupiter.api.Test;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.boot.test.context.SpringBootTest;
import org.springframework.boot.test.web.client.TestRestTemplate;
import org.springframework.http.HttpStatus;
import org.springframework.http.ResponseEntity;

/**
 * End-to-end test of the submit -> poll -> results flow against the real
 * C++ engine binary and the bundled AAPL data (H2 database).
 */
@SpringBootTest(webEnvironment = SpringBootTest.WebEnvironment.RANDOM_PORT)
class BacktestFlowIntegrationTest {

    @Autowired
    private TestRestTemplate rest;

    private static final Map<String, Object> VALID_REQUEST = Map.of(
            "strategyId", "sma_crossover",
            "params", Map.of("fast_period", 10, "slow_period", 50),
            "symbol", "AAPL",
            "startDate", "2015-01-01",
            "endDate", "2024-12-31",
            "initialCapital", 100000,
            "commissionBps", 5,
            "slippageBps", 2,
            "positionFraction", 0.95);

    @Test
    void submitPollAndFetchResults() {
        ResponseEntity<JsonNode> submitted =
                rest.postForEntity("/api/backtests", VALID_REQUEST, JsonNode.class);
        assertThat(submitted.getStatusCode()).isEqualTo(HttpStatus.ACCEPTED);
        String id = submitted.getBody().get("id").asText();
        assertThat(submitted.getBody().get("status").asText()).isIn("QUEUED", "RUNNING");

        JsonNode job = awaitCompletion(id);
        assertThat(job.get("status").asText()).isEqualTo("COMPLETED");

        JsonNode metrics = job.get("results").get("metrics");
        assertThat(metrics.get("num_trades").asInt()).isGreaterThan(0);
        assertThat(metrics.get("final_equity").asDouble()).isGreaterThan(0.0);
        assertThat(job.get("results").get("equity_curve").size()).isGreaterThan(2000);

        // The job appears in the list with its headline metrics.
        JsonNode list = rest.getForObject("/api/backtests?size=50", JsonNode.class);
        assertThat(list.get("content").findValuesAsText("id")).contains(id);
    }

    @Test
    void identicalConfigsProduceIdenticalResults() {
        String first = runToCompletion().get("results").get("metrics").toString();
        String second = runToCompletion().get("results").get("metrics").toString();
        assertThat(first).isEqualTo(second);
    }

    @Test
    void invalidParamsAreRejectedWithClearMessage() {
        ResponseEntity<JsonNode> response = rest.postForEntity("/api/backtests",
                Map.of("strategyId", "sma_crossover",
                        "params", Map.of("fast_period", 10),  // slow_period missing
                        "symbol", "AAPL",
                        "startDate", "2015-01-01",
                        "endDate", "2024-12-31",
                        "initialCapital", 100000),
                JsonNode.class);
        assertThat(response.getStatusCode()).isEqualTo(HttpStatus.BAD_REQUEST);
        assertThat(response.getBody().get("error").asText()).contains("slow_period");
    }

    @Test
    void unknownSymbolIsRejected() {
        ResponseEntity<JsonNode> response = rest.postForEntity("/api/backtests",
                Map.of("strategyId", "buy_hold",
                        "params", Map.of(),
                        "symbol", "NOSUCH",
                        "startDate", "2015-01-01",
                        "endDate", "2024-12-31",
                        "initialCapital", 100000),
                JsonNode.class);
        assertThat(response.getStatusCode()).isEqualTo(HttpStatus.BAD_REQUEST);
        assertThat(response.getBody().get("error").asText()).contains("NOSUCH");
    }

    @Test
    void deletedJobIsGone() {
        String id = runToCompletion().get("id").asText();
        rest.delete("/api/backtests/" + id);
        ResponseEntity<JsonNode> after =
                rest.getForEntity("/api/backtests/" + id, JsonNode.class);
        assertThat(after.getStatusCode()).isEqualTo(HttpStatus.NOT_FOUND);
    }

    private JsonNode runToCompletion() {
        JsonNode submitted = rest.postForObject("/api/backtests", VALID_REQUEST, JsonNode.class);
        return awaitCompletion(submitted.get("id").asText());
    }

    private JsonNode awaitCompletion(String id) {
        Instant deadline = Instant.now().plus(Duration.ofSeconds(30));
        while (Instant.now().isBefore(deadline)) {
            JsonNode job = rest.getForObject("/api/backtests/" + id, JsonNode.class);
            String status = job.get("status").asText();
            if (status.equals("COMPLETED") || status.equals("FAILED")) {
                return job;
            }
            try {
                Thread.sleep(200);
            } catch (InterruptedException e) {
                Thread.currentThread().interrupt();
                throw new IllegalStateException(e);
            }
        }
        throw new AssertionError("job " + id + " did not finish within 30s");
    }
}
