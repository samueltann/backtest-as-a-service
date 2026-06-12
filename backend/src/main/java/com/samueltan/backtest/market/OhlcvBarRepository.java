package com.samueltan.backtest.market;

import java.time.LocalDate;
import java.util.List;
import org.springframework.data.jpa.repository.JpaRepository;
import org.springframework.data.jpa.repository.Query;

public interface OhlcvBarRepository extends JpaRepository<OhlcvBar, Long> {

    List<OhlcvBar> findBySymbolAndDateBetweenOrderByDate(String symbol, LocalDate from, LocalDate to);

    List<OhlcvBar> findBySymbolOrderByDate(String symbol);

    long countBySymbol(String symbol);

    boolean existsBySymbol(String symbol);

    void deleteBySymbol(String symbol);

    @Query("""
            select b.symbol as symbol, min(b.date) as minDate, max(b.date) as maxDate, count(b) as barCount
            from OhlcvBar b group by b.symbol order by b.symbol
            """)
    List<TickerSummary> summarize();

    interface TickerSummary {
        String getSymbol();
        LocalDate getMinDate();
        LocalDate getMaxDate();
        long getBarCount();
    }
}
