package com.samueltan.backtest.engine;

import org.springframework.boot.context.properties.ConfigurationProperties;

/**
 * Locations and limits for the C++ engine integration.
 */
@ConfigurationProperties(prefix = "engine")
public record EngineProperties(
        String binaryPath,
        String bundledCsvDir,
        String cacheDir,
        String workDir,
        long timeoutSeconds) {
}
