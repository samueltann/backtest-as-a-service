# Architecture

```
┌─────────────┐   REST/JSON (JWT)   ┌──────────────────────────────┐
│  React SPA   │ ◄────────────────► │       Spring Boot API         │
│  (Vite + TS) │   polling status    │                              │
└─────────────┘                     │  AuthController  ──► JWT      │
                                    │  BacktestController           │
                                    │  TickerController             │
                                    │  MarketDataController         │
                                    │        │                      │
                                    │  BacktestService              │
                                    │        │ enqueue              │
                                    │  ThreadPoolTaskExecutor       │
                                    │   (4 workers, queue cap 100)  │
                                    │        │ per job              │
                                    │  BacktestWorker ─ EngineRunner┼──► spawns
                                    └───────┬──────────────┬────────┘
                                            │              │
                                     ┌──────▼─────┐  ┌─────▼──────────────┐
                                     │ Postgres/H2 │  │  C++ Engine (proc) │
                                     │ users, jobs,│  │  config.json  ──►  │
                                     │ results,    │  │  event loop   ──►  │
                                     │ OHLCV bars  │  │  results.json      │
                                     └─────────────┘  └────────▲───────────┘
                                                               │ reads
                                                      ┌────────┴───────┐
                                                      │ CSV data cache │
                                                      │ (per symbol)   │
                                                      └────────────────┘
```

## Job lifecycle

1. `POST /api/backtests` — request is validated (Bean Validation for shape, the strategy catalog for params, the bars table for the symbol), persisted as a `BacktestJob` in state `QUEUED`, and handed to the executor. The response is `202` with the job id.
2. A worker picks it up: state → `RUNNING`, the symbol's CSV cache is exported from the DB if missing, a `config.json` is written to a per-job work directory, and the engine binary is spawned with a 60 s hard timeout.
3. Engine exit 0: `results.json` is parsed; headline metrics land in `backtest_results` columns, the full document (equity curve + trades) in a JSON column; state → `COMPLETED`. Exit ≠ 0 or timeout: the engine's stderr JSON (`{"error": ...}`) becomes the job's `errorMessage`; state → `FAILED`.
4. The frontend polls `GET /api/backtests/{id}` (1 s) until the state is terminal. The work directory keeps the exact config for reproducibility.

On startup, jobs found `QUEUED`/`RUNNING` are marked `FAILED` ("server restarted") since the queue is in-memory.

## C++ engine (`engine/`)

Single-threaded event loop over a FIFO queue. Per bar, in order:

1. **Fills first** — orders queued on the previous bar fill at this bar's open (`SimulatedExecutionHandler::on_new_bar`), with slippage applied against the trader and commission in bps of notional.
2. **MarketEvent** — the strategy sees the bar, updates its indicators, and may emit a `SignalEvent` (Long/Exit); the portfolio sizes it into an `OrderEvent` (fixed fraction of equity at the signal close, long-only); the execution handler queues it for the *next* bar.
3. **Mark to market** — equity (cash + position × close) is recorded.

Components: `CsvDataHandler` (chronological bar stream), `Strategy` + factory (5 built-ins, parameter schemas exported via `--list-strategies`), `Portfolio` (cash/position/equity curve/round-trip trade log, affordability-clamped fills), `SimulatedExecutionHandler`, `MetricsCalculator` (CAGR by calendar time, annualized Sharpe/Sortino, max drawdown + duration, win rate, profit factor).

CLI contract: `backtest-engine --config <json> [--output <json>]`; errors go to stderr as `{"error": "..."}` with exit 1. Dependencies are vendored (nlohmann/json, Catch2) — a C++20 compiler and CMake are the only requirements.

## Backend (`backend/`)

Spring Boot 3.5 / Java 17, layered controller → service → repository (Spring Data JPA).

- `auth/` — register/login (BCrypt + HS256 JWT), `JwtAuthFilter` puts the verified user id in the security context. Strategies/tickers are public GETs; everything else requires a token.
- `backtest/` — job + result entities, the submission service (validation, persistence, enqueue, backpressure → 429), the worker, and per-user-scoped queries.
- `market/` — bars table seeded from `data/csv/` on first start; per-symbol CSV export cache for the engine; refresh endpoint pulling from Yahoo Finance.
- `engine/` — `EngineRunner` (ProcessBuilder, per-job workdir, timeout, stderr capture) and `--list-strategies` invocation.
- `strategy/` — caches the engine's catalog; validates submitted params against it.

Profiles: default = H2 file DB (`backend/data/h2/`), `postgres` = Postgres via env vars (used by compose).

## Frontend (`frontend/`)

React 18 + TypeScript (Vite), TanStack Query for data fetching (conditional `refetchInterval` implements the polling), React Router with an auth guard, Tailwind for styling, Recharts for charts. The new-backtest form is rendered from the strategy catalog, so engine strategies appear automatically. Dev server proxies `/api` to :8080; in Docker, nginx does the same, so the API is always same-origin.

## Deployment

`docker compose up --build`: Postgres (healthchecked) → backend (multi-stage image: gcc builds the engine, Temurin builds the jar, both ship on a JRE base with the seed CSVs) → frontend (Node build, nginx serve + proxy) on :3000.
