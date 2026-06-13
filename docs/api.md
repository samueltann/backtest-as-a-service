# API reference

Base URL: `/api`. All request/response bodies are JSON. Errors use a consistent shape:

```json
{ "error": "human-readable message" }
```

Authenticated endpoints require `Authorization: Bearer <jwt>`. A missing/invalid token on a protected route returns `403`.

## Auth

### `POST /api/auth/register`
```json
{ "email": "you@example.com", "password": "min 8 chars" }
```
`200` → `{ "token": "...", "email": "..." }`. `409` if the email is taken, `400` on validation failure.

### `POST /api/auth/login`
Same body. `200` → `{ "token", "email" }`. `401` on bad credentials.

## Strategies & tickers (public)

### `GET /api/strategies`
Catalog sourced from the engine. Drives the UI form and server-side validation.
```json
[{ "id": "sma_crossover", "name": "SMA Crossover", "description": "...",
   "params": [{ "name": "fast_period", "label": "Fast period", "type": "int",
                "default": 10, "min": 2, "max": 500 }, ...] }, ...]
```

### `GET /api/tickers`
```json
[{ "symbol": "AAPL", "minDate": "2011-06-13", "maxDate": "2026-06-12", "barCount": 3773 }, ...]
```

### `GET /api/tickers/{symbol}/bars?from=YYYY-MM-DD&to=YYYY-MM-DD`
Daily OHLCV in range. `from`/`to` optional.

## Backtests (auth required, scoped to the caller)

### `POST /api/backtests`
```json
{
  "strategyId": "sma_crossover",
  "params": { "fast_period": 10, "slow_period": 50 },
  "symbol": "AAPL",
  "startDate": "2015-01-01",
  "endDate": "2024-12-31",
  "initialCapital": 100000,
  "commissionBps": 5,
  "slippageBps": 2,
  "positionFraction": 0.95
}
```
`202 Accepted` → the job (status `QUEUED`). `400` on invalid params/symbol/dates, `429` if the queue is full.

### `GET /api/backtests/{id}`
The job. While `QUEUED`/`RUNNING`, `results` is `null` — poll until `status` is `COMPLETED` or `FAILED`.
```json
{
  "id": "uuid", "strategyId": "sma_crossover", "params": {...}, "symbol": "AAPL",
  "startDate": "...", "endDate": "...", "initialCapital": 100000,
  "status": "COMPLETED", "errorMessage": null,
  "createdAt": "...", "startedAt": "...", "finishedAt": "...",
  "results": {
    "metrics": { "total_return": 2.84, "cagr": 0.144, "sharpe": 0.83, "sortino": 1.24,
                 "max_drawdown": 0.263, "max_drawdown_days": 514, "win_rate": 0.45,
                 "profit_factor": 2.96, "num_trades": 33, "final_equity": 384123.2 },
    "equity_curve": [["2015-01-02", 100000.0], ...],
    "trades": [{ "entry_date": "...", "exit_date": "...", "quantity": 3041,
                 "entry_price": 31.5, "exit_price": 31.7, "pnl": 694.1, "return_pct": 0.0072 }, ...]
  }
}
```
`profit_factor` of `-1` encodes infinity (no losing trades). `404` if the id isn't yours or doesn't exist.

### `GET /api/backtests?page=0&size=20`
Paged summaries (newest first), each with headline metrics but not the full curve/trades.

### `DELETE /api/backtests/{id}`
`204`. `404` if not yours.

## Market data (auth required)

### `POST /api/market-data/{symbol}/refresh`
Pulls 15y of daily OHLCV from Yahoo Finance, replaces the symbol's bars, invalidates the engine cache.
`200` → `{ "symbol": "KO", "bars": 3773 }`. `400` for an unknown symbol.
