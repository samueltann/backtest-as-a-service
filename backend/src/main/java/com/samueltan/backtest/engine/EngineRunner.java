package com.samueltan.backtest.engine;

import com.fasterxml.jackson.databind.JsonNode;
import com.fasterxml.jackson.databind.ObjectMapper;
import java.io.IOException;
import java.io.UncheckedIOException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.List;
import java.util.concurrent.TimeUnit;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.stereotype.Component;

/**
 * Runs the C++ engine as a child process: one process per backtest.
 *
 * The process boundary is deliberate — an engine crash or hang can never take
 * down the JVM. Each job gets its own working directory containing the exact
 * config.json it ran with (reproducibility) and the results.json it produced.
 */
@Component
public class EngineRunner {

    private static final Logger log = LoggerFactory.getLogger(EngineRunner.class);

    private final EngineProperties properties;
    private final ObjectMapper mapper;

    public EngineRunner(EngineProperties properties, ObjectMapper mapper) {
        this.properties = properties;
        this.mapper = mapper;
    }

    /** Result of an engine run: exactly one of results / errorMessage is set. */
    public record EngineResult(JsonNode results, String errorMessage) {
        public boolean isSuccess() {
            return results != null;
        }
    }

    public EngineResult run(String jobId, JsonNode config) {
        try {
            Path workDir = Files.createDirectories(Path.of(properties.workDir()).resolve(jobId));
            Path configFile = workDir.resolve("config.json");
            Path resultsFile = workDir.resolve("results.json");
            Path stderrFile = workDir.resolve("stderr.log");
            mapper.writerWithDefaultPrettyPrinter().writeValue(configFile.toFile(), config);

            // Redirect stderr to a file rather than draining the pipe ourselves:
            // a child that fills the OS pipe buffer before we read would block,
            // and reading only after waitFor() would deadlock. The file also
            // stays in the work dir next to config.json/results.json for debugging.
            Process process = new ProcessBuilder(
                    Path.of(properties.binaryPath()).toAbsolutePath().toString(),
                    "--config", configFile.toString(),
                    "--output", resultsFile.toString())
                    .directory(workDir.toFile())
                    .redirectError(stderrFile.toFile())
                    .start();

            if (!process.waitFor(properties.timeoutSeconds(), TimeUnit.SECONDS)) {
                process.destroyForcibly();
                log.warn("Engine timed out for job {}", jobId);
                return new EngineResult(null,
                        "engine timed out after " + properties.timeoutSeconds() + "s");
            }

            if (process.exitValue() != 0) {
                String stderr = Files.exists(stderrFile) ? Files.readString(stderrFile) : "";
                return new EngineResult(null, parseEngineError(stderr, process.exitValue()));
            }
            return new EngineResult(mapper.readTree(resultsFile.toFile()), null);
        } catch (IOException e) {
            throw new UncheckedIOException("failed to run engine", e);
        } catch (InterruptedException e) {
            Thread.currentThread().interrupt();
            return new EngineResult(null, "engine run interrupted");
        }
    }

    /** Asks the engine for its strategy catalog ({@code --list-strategies}). */
    public JsonNode listStrategies() {
        try {
            Process process = new ProcessBuilder(
                    Path.of(properties.binaryPath()).toAbsolutePath().toString(),
                    "--list-strategies").start();
            byte[] stdout;
            try (var out = process.getInputStream()) {
                if (!process.waitFor(10, TimeUnit.SECONDS)) {
                    process.destroyForcibly();
                    throw new IllegalStateException("engine --list-strategies timed out");
                }
                stdout = out.readAllBytes();
            }
            if (process.exitValue() != 0) {
                throw new IllegalStateException("engine --list-strategies failed");
            }
            return mapper.readTree(stdout);
        } catch (IOException e) {
            throw new UncheckedIOException(
                    "cannot execute engine binary at " + properties.binaryPath()
                            + " — build it with: cmake -S engine -B engine/build && cmake --build engine/build", e);
        } catch (InterruptedException e) {
            Thread.currentThread().interrupt();
            throw new IllegalStateException("interrupted while listing strategies", e);
        }
    }

    /** The engine reports failures as {"error": "..."} on stderr. */
    private String parseEngineError(String stderr, int exitCode) {
        try {
            JsonNode node = mapper.readTree(stderr);
            if (node.hasNonNull("error")) {
                return node.get("error").asText();
            }
        } catch (IOException ignored) {
            // fall through: not JSON (e.g. crash output)
        }
        String trimmed = stderr.strip();
        List<String> parts = trimmed.isEmpty()
                ? List.of("engine exited with code " + exitCode)
                : List.of("engine exited with code " + exitCode, trimmed);
        return String.join(": ", parts);
    }
}
