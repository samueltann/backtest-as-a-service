# Backtest as a Service

A full-stack platform for backtesting trading strategies: a **React** frontend and **Spring Boot** REST API orchestrating a **C++ event-driven backtesting engine**, with JWT auth, an async job queue, and per-user result history.

```
React (Vite + TS)  ──REST/JSON──>  Spring Boot API  ──process-per-job──>  C++ engine
     Recharts                      JWT · job queue                        event loop
                                   Postgres / H2                          CSV market data
```

## What it does

- Pick one of five built-in strategies (SMA crossover, EMA momentum, Bollinger mean-reversion, RSI reversion, buy-and-hold benchmark), tune its parameters, choose a ticker / date range / cost model, and submit.
- The backend queues the job (`202 Accepted`) and runs it on a bounded worker pool, spawning the C++ engine as an isolated process per backtest. The UI polls until the job completes.
- Results: equity curve, drawdown chart, trade log, and metrics (total return, CAGR, Sharpe, Sortino, max drawdown + duration, win rate, profit factor).
- Compare up to four runs overlaid as growth-of-$1 with a side-by-side metrics table.
- 15 years of daily OHLCV for 21 liquid tickers ships in the repo; a refresh endpoint pulls fresh data per symbol.

## Quick start

**Docker (one command):**

```sh
docker compose up --build
# UI on http://localhost:3000, API on http://localhost:8080
```

**Local development:**

```sh
# 1. Engine (C++20, CMake)
cmake -S engine -B engine/build -DCMAKE_BUILD_TYPE=Release
cmake --build engine/build -j
ctest --test-dir engine/build            # run engine tests

# 2. Backend (Java 17) — H2 database, no other services needed
cd backend && ./mvnw spring-boot:run     # http://localhost:8080

# 3. Frontend (Node 20)
cd frontend && npm install && npm run dev  # http://localhost:5173, proxies /api
```

## Why it's built this way

The interesting design decisions — process-per-job vs JNI, lookahead-bias prevention, the engine as the single source of truth for the strategy catalog, bounded queues and backpressure — are written up in [docs/decisions.md](docs/decisions.md). Architecture details are in [docs/architecture.md](docs/architecture.md) and the API surface in [docs/api.md](docs/api.md).

## Repository layout

```
engine/     C++20 event-driven backtesting engine (CMake, Catch2 tests)
backend/    Spring Boot 3.5 REST API (Maven, JUnit integration tests)
frontend/   React 18 + TypeScript SPA (Vite, Tailwind, Recharts)
data/csv/   Bundled daily OHLCV (21 tickers, ~15 years, via Yahoo Finance)
scripts/    Data fetcher and dev helpers
docs/       Architecture, API, and design-decision write-ups
```

## Testing

| Layer    | What's covered |
|----------|----------------|
| Engine   | Indicator math, hand-computed fills/PnL on synthetic series, a no-lookahead regression test, metrics on fixed fixtures, error paths |
| Backend  | Full submit → poll → results flow against the real engine binary, reproducibility (same config ⇒ identical results), auth (token issuance, cross-user isolation), validation errors |
| Frontend | Type-checked build; exercised end-to-end against the live stack |

CI runs all of the above plus a Docker image build on every push.
