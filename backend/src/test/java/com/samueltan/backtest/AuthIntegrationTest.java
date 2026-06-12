package com.samueltan.backtest;

import static org.assertj.core.api.Assertions.assertThat;

import com.fasterxml.jackson.databind.JsonNode;
import java.util.Map;
import java.util.UUID;
import org.junit.jupiter.api.Test;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.boot.test.context.SpringBootTest;
import org.springframework.boot.test.web.client.TestRestTemplate;
import org.springframework.http.HttpStatus;
import org.springframework.http.ResponseEntity;

@SpringBootTest(webEnvironment = SpringBootTest.WebEnvironment.RANDOM_PORT)
class AuthIntegrationTest {

    @Autowired
    private TestRestTemplate rest;

    private String uniqueEmail() {
        return "auth-" + UUID.randomUUID() + "@test.com";
    }

    @Test
    void registerThenLoginIssuesTokens() {
        String email = uniqueEmail();
        ResponseEntity<JsonNode> registered = rest.postForEntity("/api/auth/register",
                Map.of("email", email, "password", "password123"), JsonNode.class);
        assertThat(registered.getStatusCode()).isEqualTo(HttpStatus.OK);
        assertThat(registered.getBody().get("token").asText()).isNotBlank();
        assertThat(registered.getBody().get("email").asText()).isEqualTo(email);

        ResponseEntity<JsonNode> loggedIn = rest.postForEntity("/api/auth/login",
                Map.of("email", email, "password", "password123"), JsonNode.class);
        assertThat(loggedIn.getStatusCode()).isEqualTo(HttpStatus.OK);
        assertThat(loggedIn.getBody().get("token").asText()).isNotBlank();
    }

    @Test
    void duplicateEmailIsConflict() {
        String email = uniqueEmail();
        rest.postForEntity("/api/auth/register",
                Map.of("email", email, "password", "password123"), JsonNode.class);
        ResponseEntity<JsonNode> again = rest.postForEntity("/api/auth/register",
                Map.of("email", email, "password", "password123"), JsonNode.class);
        assertThat(again.getStatusCode()).isEqualTo(HttpStatus.CONFLICT);
    }

    @Test
    void wrongPasswordIsUnauthorized() {
        String email = uniqueEmail();
        rest.postForEntity("/api/auth/register",
                Map.of("email", email, "password", "password123"), JsonNode.class);
        ResponseEntity<JsonNode> response = rest.postForEntity("/api/auth/login",
                Map.of("email", email, "password", "wrong-password"), JsonNode.class);
        assertThat(response.getStatusCode()).isEqualTo(HttpStatus.UNAUTHORIZED);
        assertThat(response.getBody().get("error").asText()).contains("invalid");
    }

    @Test
    void weakPasswordIsRejected() {
        ResponseEntity<JsonNode> response = rest.postForEntity("/api/auth/register",
                Map.of("email", uniqueEmail(), "password", "short"), JsonNode.class);
        assertThat(response.getStatusCode()).isEqualTo(HttpStatus.BAD_REQUEST);
    }

    @Test
    void publicEndpointsRemainPublic() {
        assertThat(rest.getForEntity("/api/strategies", JsonNode.class).getStatusCode())
                .isEqualTo(HttpStatus.OK);
        assertThat(rest.getForEntity("/api/tickers", JsonNode.class).getStatusCode())
                .isEqualTo(HttpStatus.OK);
    }

    @Test
    void garbageTokenIsRejected() {
        var headers = new org.springframework.http.HttpHeaders();
        headers.setBearerAuth("not.a.jwt");
        ResponseEntity<JsonNode> response = rest.exchange("/api/backtests",
                org.springframework.http.HttpMethod.GET,
                new org.springframework.http.HttpEntity<>(headers), JsonNode.class);
        assertThat(response.getStatusCode()).isEqualTo(HttpStatus.FORBIDDEN);
    }
}
