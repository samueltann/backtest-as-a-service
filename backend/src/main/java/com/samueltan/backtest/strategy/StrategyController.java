package com.samueltan.backtest.strategy;

import com.fasterxml.jackson.databind.JsonNode;
import org.springframework.web.bind.annotation.GetMapping;
import org.springframework.web.bind.annotation.RequestMapping;
import org.springframework.web.bind.annotation.RestController;

@RestController
@RequestMapping("/api/strategies")
public class StrategyController {

    private final StrategyCatalogService catalogService;

    public StrategyController(StrategyCatalogService catalogService) {
        this.catalogService = catalogService;
    }

    @GetMapping
    public JsonNode list() {
        return catalogService.catalog();
    }
}
