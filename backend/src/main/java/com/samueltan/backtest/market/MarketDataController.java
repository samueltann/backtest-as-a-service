package com.samueltan.backtest.market;

import org.springframework.web.bind.annotation.PathVariable;
import org.springframework.web.bind.annotation.PostMapping;
import org.springframework.web.bind.annotation.RequestMapping;
import org.springframework.web.bind.annotation.RestController;

/** Mutating market-data operations (auth required by the security config). */
@RestController
@RequestMapping("/api/market-data")
public class MarketDataController {

    private final MarketDataFetchService fetchService;

    public MarketDataController(MarketDataFetchService fetchService) {
        this.fetchService = fetchService;
    }

    @PostMapping("/{symbol}/refresh")
    public MarketDataFetchService.RefreshOutcome refresh(@PathVariable String symbol) {
        return fetchService.refresh(symbol);
    }
}
