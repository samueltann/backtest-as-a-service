package com.samueltan.backtest.market;

import java.time.LocalDate;
import java.util.List;
import org.springframework.web.bind.annotation.GetMapping;
import org.springframework.web.bind.annotation.PathVariable;
import org.springframework.web.bind.annotation.RequestMapping;
import org.springframework.web.bind.annotation.RequestParam;
import org.springframework.web.bind.annotation.RestController;

@RestController
@RequestMapping("/api/tickers")
public class TickerController {

    private final MarketDataService marketData;

    public TickerController(MarketDataService marketData) {
        this.marketData = marketData;
    }

    public record TickerDto(String symbol, LocalDate minDate, LocalDate maxDate, long barCount) {
    }

    public record BarDto(LocalDate date, double open, double high, double low,
                         double close, long volume) {
    }

    @GetMapping
    public List<TickerDto> list() {
        return marketData.listTickers().stream()
                .map(t -> new TickerDto(t.getSymbol(), t.getMinDate(), t.getMaxDate(), t.getBarCount()))
                .toList();
    }

    @GetMapping("/{symbol}/bars")
    public List<BarDto> bars(@PathVariable String symbol,
                             @RequestParam(defaultValue = "1900-01-01") LocalDate from,
                             @RequestParam(defaultValue = "2100-01-01") LocalDate to) {
        return marketData.bars(symbol.toUpperCase(), from, to).stream()
                .map(b -> new BarDto(b.getDate(), b.getOpen(), b.getHigh(), b.getLow(),
                        b.getClose(), b.getVolume()))
                .toList();
    }
}
