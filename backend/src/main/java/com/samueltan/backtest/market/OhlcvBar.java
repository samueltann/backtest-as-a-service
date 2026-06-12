package com.samueltan.backtest.market;

import jakarta.persistence.Entity;
import jakarta.persistence.GeneratedValue;
import jakarta.persistence.GenerationType;
import jakarta.persistence.Id;
import jakarta.persistence.Index;
import jakarta.persistence.Table;
import jakarta.persistence.UniqueConstraint;
import java.time.LocalDate;

/**
 * One daily OHLCV bar. The database is the source of truth for market data;
 * per-symbol CSV files are exported from here for the engine to read.
 */
@Entity
@Table(name = "ohlcv_bars",
        uniqueConstraints = @UniqueConstraint(columnNames = {"symbol", "bar_date"}),
        indexes = @Index(name = "idx_symbol_date", columnList = "symbol, bar_date"))
public class OhlcvBar {

    @Id
    @GeneratedValue(strategy = GenerationType.IDENTITY)
    private Long id;

    private String symbol;

    @jakarta.persistence.Column(name = "bar_date")
    private LocalDate date;

    private double open;
    private double high;
    private double low;
    private double close;
    private long volume;

    protected OhlcvBar() {
    }

    public OhlcvBar(String symbol, LocalDate date, double open, double high,
                    double low, double close, long volume) {
        this.symbol = symbol;
        this.date = date;
        this.open = open;
        this.high = high;
        this.low = low;
        this.close = close;
        this.volume = volume;
    }

    public Long getId() { return id; }
    public String getSymbol() { return symbol; }
    public LocalDate getDate() { return date; }
    public double getOpen() { return open; }
    public double getHigh() { return high; }
    public double getLow() { return low; }
    public double getClose() { return close; }
    public long getVolume() { return volume; }
}
