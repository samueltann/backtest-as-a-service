package com.samueltan.backtest.market;

import org.springframework.boot.ApplicationArguments;
import org.springframework.boot.ApplicationRunner;
import org.springframework.stereotype.Component;

/** Seeds the database from the bundled CSVs on first startup. */
@Component
public class DataImportRunner implements ApplicationRunner {

    private final MarketDataService marketData;

    public DataImportRunner(MarketDataService marketData) {
        this.marketData = marketData;
    }

    @Override
    public void run(ApplicationArguments args) {
        marketData.importBundledCsvsIfEmpty();
    }
}
