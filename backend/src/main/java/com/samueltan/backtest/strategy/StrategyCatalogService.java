package com.samueltan.backtest.strategy;

import com.fasterxml.jackson.databind.JsonNode;
import com.samueltan.backtest.engine.EngineRunner;
import java.util.Map;
import org.springframework.stereotype.Service;

/**
 * Caches the strategy catalog the engine reports via --list-strategies.
 * The engine is the single source of truth for which strategies exist and
 * what parameters they take; the backend validates requests against this
 * schema and the frontend renders its forms from it.
 */
@Service
public class StrategyCatalogService {

    private final EngineRunner engineRunner;
    private volatile JsonNode catalog;

    public StrategyCatalogService(EngineRunner engineRunner) {
        this.engineRunner = engineRunner;
    }

    public JsonNode catalog() {
        JsonNode local = catalog;
        if (local == null) {
            synchronized (this) {
                if (catalog == null) {
                    catalog = engineRunner.listStrategies();
                }
                local = catalog;
            }
        }
        return local;
    }

    public JsonNode findStrategy(String id) {
        for (JsonNode strategy : catalog()) {
            if (strategy.get("id").asText().equals(id)) {
                return strategy;
            }
        }
        throw new InvalidStrategyException("unknown strategy: " + id);
    }

    /**
     * Validates user-supplied params against the strategy's schema: every
     * declared param must be present, numeric, and within [min, max]; no
     * undeclared params are allowed.
     */
    public void validateParams(String strategyId, Map<String, Object> params) {
        JsonNode schema = findStrategy(strategyId).get("params");

        for (JsonNode param : schema) {
            String name = param.get("name").asText();
            Object value = params.get(name);
            if (!(value instanceof Number number)) {
                throw new InvalidStrategyException("missing or non-numeric param: " + name);
            }
            double v = number.doubleValue();
            if ("int".equals(param.get("type").asText()) && v != Math.floor(v)) {
                throw new InvalidStrategyException("param " + name + " must be an integer");
            }
            if (v < param.get("min").asDouble() || v > param.get("max").asDouble()) {
                throw new InvalidStrategyException("param " + name + " out of range ["
                        + param.get("min").asDouble() + ", " + param.get("max").asDouble() + "]");
            }
        }

        for (String key : params.keySet()) {
            boolean declared = false;
            for (JsonNode param : schema) {
                if (param.get("name").asText().equals(key)) {
                    declared = true;
                    break;
                }
            }
            if (!declared) {
                throw new InvalidStrategyException("unexpected param: " + key);
            }
        }
    }

    public static class InvalidStrategyException extends RuntimeException {
        public InvalidStrategyException(String message) {
            super(message);
        }
    }
}
