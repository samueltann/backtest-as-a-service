package com.samueltan.backtest;

import static org.assertj.core.api.Assertions.assertThat;

import com.fasterxml.jackson.databind.JsonNode;
import java.time.Duration;
import java.time.Instant;
import java.util.Map;
import java.util.UUID;
import org.junit.jupiter.api.BeforeEach;
import org.junit.jupiter.api.Test;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.boot.test.context.SpringBootTest;
import org.springframework.boot.test.web.client.TestRestTemplate;
import org.springframework.http.HttpEntity;
import org.springframework.http.HttpHeaders;
import org.springframework.http.HttpMethod;
import org.springframework.http.HttpStatus;
import org.springframework.http.ResponseEntity;

/**
 * End-to-end tests of the authenticated submit -> poll -> results flow
 * against the real C++ engine binary and the bundled AAPL data (H2).
 */
@SpringBootTest(webEnvironment = SpringBootTest.WebEnvironment.RANDOM_PORT)
class BacktestFlowIntegrationTest {

    @Autowired
    private TestRestTemplate rest;

    private String token;

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

    @BeforeEach
    void registerUser() {
        token = registerAndGetToken("user-" + UUID.randomUUID() + "@test.com");
    }

    @Test
    void submitPollAndFetchResults() {
        ResponseEntity<JsonNode> submitted = post("/api/backtests", VALID_REQUEST, token);
        assertThat(submitted.getStatusCode()).isEqualTo(HttpStatus.ACCEPTED);
        String id = submitted.getBody().get("id").asText();
        assertThat(submitted.getBody().get("status").asText()).isIn("QUEUED", "RUNNING");

        JsonNode job = awaitCompletion(id, token);
        assertThat(job.get("status").asText()).isEqualTo("COMPLETED");

        JsonNode metrics = job.get("results").get("metrics");
        assertThat(metrics.get("num_trades").asInt()).isGreaterThan(0);
        assertThat(metrics.get("final_equity").asDouble()).isGreaterThan(0.0);
        assertThat(job.get("results").get("equity_curve").size()).isGreaterThan(2000);

        JsonNode list = get("/api/backtests?size=50", token).getBody();
        assertThat(list.get("content").findValuesAsText("id")).contains(id);
    }

    @Test
    void identicalConfigsProduceIdenticalResults() {
        String first = runToCompletion().get("results").get("metrics").toString();
        String second = runToCompletion().get("results").get("metrics").toString();
        assertThat(first).isEqualTo(second);
    }

    @Test
    void requestsWithoutTokenAreRejected() {
        ResponseEntity<JsonNode> response = post("/api/backtests", VALID_REQUEST, null);
        assertThat(response.getStatusCode()).isEqualTo(HttpStatus.FORBIDDEN);
    }

    @Test
    void usersCannotSeeEachOthersJobs() {
        String id = runToCompletion().get("id").asText();

        String otherToken = registerAndGetToken("other-" + UUID.randomUUID() + "@test.com");
        assertThat(get("/api/backtests/" + id, otherToken).getStatusCode())
                .isEqualTo(HttpStatus.NOT_FOUND);
        assertThat(get("/api/backtests", otherToken).getBody()
                .get("content").findValuesAsText("id")).doesNotContain(id);

        // ...but the owner still can.
        assertThat(get("/api/backtests/" + id, token).getStatusCode()).isEqualTo(HttpStatus.OK);
    }

    @Test
    void invalidParamsAreRejectedWithClearMessage() {
        ResponseEntity<JsonNode> response = post("/api/backtests",
                Map.of("strategyId", "sma_crossover",
                        "params", Map.of("fast_period", 10),  // slow_period missing
                        "symbol", "AAPL",
                        "startDate", "2015-01-01",
                        "endDate", "2024-12-31",
                        "initialCapital", 100000),
                token);
        assertThat(response.getStatusCode()).isEqualTo(HttpStatus.BAD_REQUEST);
        assertThat(response.getBody().get("error").asText()).contains("slow_period");
    }

    @Test
    void unknownSymbolIsRejected() {
        ResponseEntity<JsonNode> response = post("/api/backtests",
                Map.of("strategyId", "buy_hold",
                        "params", Map.of(),
                        "symbol", "NOSUCH",
                        "startDate", "2015-01-01",
                        "endDate", "2024-12-31",
                        "initialCapital", 100000),
                token);
        assertThat(response.getStatusCode()).isEqualTo(HttpStatus.BAD_REQUEST);
        assertThat(response.getBody().get("error").asText()).contains("NOSUCH");
    }

    @Test
    void engineFailureSurfacesAsFailedJobWithMessage() {
        // A valid symbol but a date range with no data passes request validation,
        // then the engine exits non-zero; the worker must surface its stderr
        // error and mark the job FAILED (exercises the stderr-from-file path).
        ResponseEntity<JsonNode> submitted = post("/api/backtests",
                Map.of("strategyId", "buy_hold",
                        "params", Map.of(),
                        "symbol", "AAPL",
                        "startDate", "1990-01-01",
                        "endDate", "1991-01-01",
                        "initialCapital", 100000),
                token);
        assertThat(submitted.getStatusCode()).isEqualTo(HttpStatus.ACCEPTED);

        JsonNode job = awaitCompletion(submitted.getBody().get("id").asText(), token);
        assertThat(job.get("status").asText()).isEqualTo("FAILED");
        assertThat(job.get("errorMessage").asText()).contains("no bars in range");
        assertThat(job.get("results").isNull()).isTrue();
    }

    @Test
    void deletedJobIsGone() {
        String id = runToCompletion().get("id").asText();
        exchange("/api/backtests/" + id, HttpMethod.DELETE, null, token);
        assertThat(get("/api/backtests/" + id, token).getStatusCode())
                .isEqualTo(HttpStatus.NOT_FOUND);
    }

    // --- helpers -------------------------------------------------------------

    private String registerAndGetToken(String email) {
        ResponseEntity<JsonNode> response = post("/api/auth/register",
                Map.of("email", email, "password", "password123"), null);
        assertThat(response.getStatusCode()).isEqualTo(HttpStatus.OK);
        return response.getBody().get("token").asText();
    }

    private JsonNode runToCompletion() {
        JsonNode submitted = post("/api/backtests", VALID_REQUEST, token).getBody();
        return awaitCompletion(submitted.get("id").asText(), token);
    }

    private JsonNode awaitCompletion(String id, String authToken) {
        Instant deadline = Instant.now().plus(Duration.ofSeconds(30));
        while (Instant.now().isBefore(deadline)) {
            JsonNode job = get("/api/backtests/" + id, authToken).getBody();
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

    private ResponseEntity<JsonNode> post(String path, Object body, String authToken) {
        return exchange(path, HttpMethod.POST, body, authToken);
    }

    private ResponseEntity<JsonNode> get(String path, String authToken) {
        return exchange(path, HttpMethod.GET, null, authToken);
    }

    private ResponseEntity<JsonNode> exchange(String path, HttpMethod method, Object body,
                                              String authToken) {
        HttpHeaders headers = new HttpHeaders();
        if (authToken != null) {
            headers.setBearerAuth(authToken);
        }
        return rest.exchange(path, method, new HttpEntity<>(body, headers), JsonNode.class);
    }
}
