package com.samueltan.backtest.market;

import com.fasterxml.jackson.databind.JsonNode;
import com.fasterxml.jackson.databind.ObjectMapper;
import java.io.IOException;
import java.net.URI;
import java.net.http.HttpClient;
import java.net.http.HttpRequest;
import java.net.http.HttpResponse;
import java.time.Duration;
import java.time.Instant;
import java.time.LocalDate;
import java.time.ZoneOffset;
import java.util.ArrayList;
import java.util.List;
import java.util.Locale;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.stereotype.Service;
import org.springframework.transaction.annotation.Transactional;

/**
 * Pulls daily OHLCV history from Yahoo Finance's public chart API and
 * replaces the symbol's bars in the database. Replace-then-reexport keeps
 * the logic simple and idempotent; the engine CSV cache is invalidated so
 * the next backtest sees the fresh data.
 */
@Service
public class MarketDataFetchService {

    private static final Logger log = LoggerFactory.getLogger(MarketDataFetchService.class);
    private static final String URL =
            "https://query1.finance.yahoo.com/v8/finance/chart/%s?range=15y&interval=1d";

    private final OhlcvBarRepository repository;
    private final MarketDataService marketData;
    private final ObjectMapper mapper;
    private final HttpClient http = HttpClient.newBuilder()
            .connectTimeout(Duration.ofSeconds(10))
            .build();

    public MarketDataFetchService(OhlcvBarRepository repository, MarketDataService marketData,
                                  ObjectMapper mapper) {
        this.repository = repository;
        this.marketData = marketData;
        this.mapper = mapper;
    }

    public record RefreshOutcome(String symbol, int bars) {
    }

    @Transactional
    public RefreshOutcome refresh(String rawSymbol) {
        String symbol = rawSymbol.strip().toUpperCase(Locale.ROOT);
        if (!symbol.matches("[A-Z0-9.\\-]{1,10}")) {
            throw new IllegalArgumentException("invalid symbol: " + rawSymbol);
        }

        List<OhlcvBar> bars = fetch(symbol);
        if (bars.isEmpty()) {
            throw new IllegalArgumentException("no data returned for symbol: " + symbol);
        }

        repository.deleteBySymbol(symbol);
        repository.flush(); // execute the delete before inserting, or the unique (symbol, date) index trips
        repository.saveAll(bars);
        marketData.invalidateCache(symbol);
        log.info("Refreshed {}: {} bars", symbol, bars.size());
        return new RefreshOutcome(symbol, bars.size());
    }

    private List<OhlcvBar> fetch(String symbol) {
        HttpRequest request = HttpRequest.newBuilder()
                .uri(URI.create(URL.formatted(symbol)))
                .header("User-Agent", "Mozilla/5.0 (backtest-as-a-service)")
                .timeout(Duration.ofSeconds(30))
                .GET()
                .build();
        try {
            HttpResponse<String> response = http.send(request, HttpResponse.BodyHandlers.ofString());
            if (response.statusCode() == 404) {
                throw new IllegalArgumentException("unknown symbol: " + symbol);
            }
            if (response.statusCode() != 200) {
                throw new IllegalStateException("market data provider returned " + response.statusCode());
            }
            return parse(symbol, mapper.readTree(response.body()));
        } catch (IOException e) {
            throw new IllegalStateException("market data provider unreachable", e);
        } catch (InterruptedException e) {
            Thread.currentThread().interrupt();
            throw new IllegalStateException("interrupted while fetching market data", e);
        }
    }

    private List<OhlcvBar> parse(String symbol, JsonNode root) {
        JsonNode result = root.path("chart").path("result").path(0);
        JsonNode timestamps = result.path("timestamp");
        JsonNode quote = result.path("indicators").path("quote").path(0);
        List<OhlcvBar> bars = new ArrayList<>();
        for (int i = 0; i < timestamps.size(); i++) {
            JsonNode open = quote.path("open").path(i);
            JsonNode high = quote.path("high").path(i);
            JsonNode low = quote.path("low").path(i);
            JsonNode close = quote.path("close").path(i);
            if (open.isNull() || high.isNull() || low.isNull() || close.isNull()) {
                continue; // provider emits null rows for halts/holidays
            }
            LocalDate date = Instant.ofEpochSecond(timestamps.get(i).asLong())
                    .atZone(ZoneOffset.UTC).toLocalDate();
            bars.add(new OhlcvBar(symbol, date, open.asDouble(), high.asDouble(),
                    low.asDouble(), close.asDouble(), quote.path("volume").path(i).asLong(0)));
        }
        return bars;
    }
}
