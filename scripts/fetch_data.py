#!/usr/bin/env python3
"""Fetch daily OHLCV history from Yahoo Finance and write engine-format CSVs.

Usage: python3 scripts/fetch_data.py [TICKER ...]
Writes data/csv/<TICKER>.csv with header Date,Open,High,Low,Close,Volume.
"""
import json
import sys
import time
import urllib.request
from datetime import datetime, timezone
from pathlib import Path

DEFAULT_TICKERS = [
    "AAPL", "MSFT", "GOOGL", "AMZN", "NVDA", "META", "TSLA",
    "JPM", "GS", "BAC", "V", "MA",
    "XOM", "CVX", "JNJ", "PG", "KO", "WMT", "DIS",
    "SPY", "QQQ",
]

OUT_DIR = Path(__file__).resolve().parent.parent / "data" / "csv"
URL = "https://query1.finance.yahoo.com/v8/finance/chart/{ticker}?range=15y&interval=1d"
HEADERS = {"User-Agent": "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7)"}


def fetch(ticker: str) -> int:
    req = urllib.request.Request(URL.format(ticker=ticker), headers=HEADERS)
    with urllib.request.urlopen(req, timeout=30) as resp:
        data = json.load(resp)

    result = data["chart"]["result"][0]
    timestamps = result["timestamp"]
    quote = result["indicators"]["quote"][0]

    rows = []
    for i, ts in enumerate(timestamps):
        o, h, l, c, v = (quote[k][i] for k in ("open", "high", "low", "close", "volume"))
        if None in (o, h, l, c):
            continue  # Yahoo emits null rows for halts/holidays
        date = datetime.fromtimestamp(ts, tz=timezone.utc).strftime("%Y-%m-%d")
        rows.append(f"{date},{o:.4f},{h:.4f},{l:.4f},{c:.4f},{int(v or 0)}")

    out = OUT_DIR / f"{ticker}.csv"
    out.write_text("Date,Open,High,Low,Close,Volume\n" + "\n".join(rows) + "\n")
    return len(rows)


def main() -> None:
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    tickers = sys.argv[1:] or DEFAULT_TICKERS
    for ticker in tickers:
        try:
            n = fetch(ticker)
            print(f"{ticker}: {n} bars")
        except Exception as e:  # noqa: BLE001 - report and continue
            print(f"{ticker}: FAILED ({e})", file=sys.stderr)
        time.sleep(0.5)  # be polite to the free endpoint


if __name__ == "__main__":
    main()
