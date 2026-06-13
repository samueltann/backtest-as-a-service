package com.samueltan.backtest.market;

import com.samueltan.backtest.engine.EngineProperties;
import java.io.IOException;
import java.io.UncheckedIOException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.StandardCopyOption;
import java.time.LocalDate;
import java.util.ArrayList;
import java.util.List;
import java.util.Locale;
import java.util.concurrent.ConcurrentHashMap;
import java.util.stream.Stream;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.jdbc.core.JdbcTemplate;
import org.springframework.stereotype.Service;
import org.springframework.transaction.annotation.Transactional;

/**
 * Owns market data. Bars live in the database (source of truth); the engine
 * reads plain CSV files, so this service exports a per-symbol CSV cache that
 * is regenerated whenever the data for that symbol changes.
 */
@Service
public class MarketDataService {

    private static final Logger log = LoggerFactory.getLogger(MarketDataService.class);
    private static final String CSV_HEADER = "Date,Open,High,Low,Close,Volume";

    private final OhlcvBarRepository repository;
    private final JdbcTemplate jdbc;
    private final EngineProperties properties;

    // One lock per symbol so concurrent workers don't redundantly rebuild — or
    // worse, interleave writes into — the same cache file.
    private final ConcurrentHashMap<String, Object> symbolLocks = new ConcurrentHashMap<>();

    public MarketDataService(OhlcvBarRepository repository, JdbcTemplate jdbc,
                             EngineProperties properties) {
        this.repository = repository;
        this.jdbc = jdbc;
        this.properties = properties;
    }

    /** Loads every bundled CSV into the database on first startup. */
    @Transactional
    public void importBundledCsvsIfEmpty() {
        if (repository.count() > 0) {
            return;
        }
        Path dir = Path.of(properties.bundledCsvDir());
        if (!Files.isDirectory(dir)) {
            log.warn("Bundled CSV directory not found: {}", dir.toAbsolutePath());
            return;
        }
        try (Stream<Path> files = Files.list(dir)) {
            files.filter(p -> p.getFileName().toString().endsWith(".csv"))
                    .sorted()
                    .forEach(this::importCsv);
        } catch (IOException e) {
            throw new UncheckedIOException(e);
        }
    }

    private void importCsv(Path csv) {
        String symbol = csv.getFileName().toString().replace(".csv", "").toUpperCase(Locale.ROOT);
        List<Object[]> rows = new ArrayList<>();
        try (Stream<String> lines = Files.lines(csv)) {
            lines.skip(1).filter(l -> !l.isBlank()).forEach(line -> {
                String[] f = line.split(",");
                rows.add(new Object[]{symbol, LocalDate.parse(f[0]),
                        Double.parseDouble(f[1]), Double.parseDouble(f[2]),
                        Double.parseDouble(f[3]), Double.parseDouble(f[4]),
                        (long) Double.parseDouble(f[5])});
            });
        } catch (IOException e) {
            throw new UncheckedIOException(e);
        }
        jdbc.batchUpdate("""
                insert into ohlcv_bars (symbol, bar_date, open, high, low, close, volume)
                values (?, ?, ?, ?, ?, ?, ?)
                """, rows);
        log.info("Imported {} bars for {}", rows.size(), symbol);
    }

    /**
     * Ensures the engine-readable CSV for a symbol exists and returns its path.
     * The cache file is written once from the database and invalidated by
     * {@link #invalidateCache}.
     */
    public Path ensureCsvForEngine(String symbol) {
        if (!repository.existsBySymbol(symbol)) {
            throw new UnknownSymbolException(symbol);
        }
        Path cacheDir = Path.of(properties.cacheDir());
        Path file = cacheDir.resolve(symbol + ".csv");
        if (Files.exists(file)) {
            return file;
        }
        // Build under a per-symbol lock so only one worker writes; the others
        // block then reuse the finished file (double-checked inside the lock).
        synchronized (symbolLocks.computeIfAbsent(symbol, k -> new Object())) {
            if (!Files.exists(file)) {
                writeCacheFile(symbol, cacheDir, file);
            }
        }
        return file;
    }

    private void writeCacheFile(String symbol, Path cacheDir, Path file) {
        try {
            Files.createDirectories(cacheDir);
            List<String> lines = new ArrayList<>();
            lines.add(CSV_HEADER);
            for (OhlcvBar bar : repository.findBySymbolOrderByDate(symbol)) {
                lines.add("%s,%.4f,%.4f,%.4f,%.4f,%d".formatted(
                        bar.getDate(), bar.getOpen(), bar.getHigh(),
                        bar.getLow(), bar.getClose(), bar.getVolume()));
            }
            // Write to a temp file in the same directory, then publish atomically
            // so the engine subprocess never reads a half-written file.
            Path tmp = Files.createTempFile(cacheDir, symbol + "-", ".csv.tmp");
            try {
                Files.write(tmp, lines);
                Files.move(tmp, file, StandardCopyOption.ATOMIC_MOVE,
                        StandardCopyOption.REPLACE_EXISTING);
            } finally {
                Files.deleteIfExists(tmp);
            }
        } catch (IOException e) {
            throw new UncheckedIOException(e);
        }
    }

    /**
     * Replaces all bars for a symbol in one transaction and invalidates its
     * engine CSV cache. Callers fetch the new data <em>before</em> calling
     * this, so no network I/O happens while the DB transaction is open.
     */
    @Transactional
    public void replaceBars(String symbol, List<OhlcvBar> bars) {
        repository.deleteBySymbol(symbol);
        repository.flush(); // run the delete before inserting, or the unique (symbol, date) index trips
        repository.saveAll(bars);
        invalidateCache(symbol);
    }

    public void invalidateCache(String symbol) {
        try {
            Files.deleteIfExists(Path.of(properties.cacheDir()).resolve(symbol + ".csv"));
        } catch (IOException e) {
            throw new UncheckedIOException(e);
        }
    }

    public List<OhlcvBarRepository.TickerSummary> listTickers() {
        return repository.summarize();
    }

    public List<OhlcvBar> bars(String symbol, LocalDate from, LocalDate to) {
        if (!repository.existsBySymbol(symbol)) {
            throw new UnknownSymbolException(symbol);
        }
        return repository.findBySymbolAndDateBetweenOrderByDate(symbol, from, to);
    }

    public static class UnknownSymbolException extends RuntimeException {
        public UnknownSymbolException(String symbol) {
            super("unknown symbol: " + symbol);
        }
    }
}
