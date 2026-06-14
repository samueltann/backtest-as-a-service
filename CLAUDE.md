# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

A three-tier trading-strategy backtesting platform in one monorepo:

- `engine/` — C++20 event-driven backtesting engine (CMake, Catch2). Does the actual computation.
- `backend/` — Spring Boot 3.5 / Java 17 REST API (Maven, JPA). Auth, job queue, market data, orchestration.
- `frontend/` — React 19 + TypeScript SPA (Vite, Tailwind v4, Recharts, TanStack Query).

The three communicate through one contract: the engine's `config.json` / `results.json` files and its `--list-strategies` catalog. That shared contract is why this is a monorepo — a contract change should land as one atomic commit across all three layers.

Deep-dive docs already exist and are worth reading before non-trivial work: [docs/architecture.md](docs/architecture.md), [docs/decisions.md](docs/decisions.md) (the "why" behind every major choice), [docs/api.md](docs/api.md) (full REST surface).

## The one thing that trips you up: build the engine first

**The backend spawns the engine binary as a child process per backtest, and its integration tests do too.** If the binary at `engine/build/backtest-engine` is missing, the backend fails at runtime and tests fail. Always build the engine before running or testing the backend:

```sh
cmake -S engine -B engine/build -DCMAKE_BUILD_TYPE=Release
cmake --build engine/build -j
```

The path is configured in [application.properties](backend/src/main/resources/application.properties) as `engine.binary-path=../engine/build/backtest-engine`, relative to `backend/` (the working dir when run via `./mvnw spring-boot:run`).

## Commands

### Engine (C++)
```sh
cmake -S engine -B engine/build -DCMAKE_BUILD_TYPE=Release
cmake --build engine/build -j
ctest --test-dir engine/build --output-on-failure   # all tests
./engine/build/bt_tests "<test name or pattern>"      # single Catch2 test/section
./engine/build/backtest-engine --list-strategies      # the strategy catalog (JSON)
./engine/build/backtest-engine --config job.json --output out.json   # run one backtest by hand
```

### Backend (Java 17, run from `backend/`)
```sh
./mvnw spring-boot:run                  # http://localhost:8080, H2 file DB, no other services
./mvnw -B test                          # all tests (needs the engine binary built first)
./mvnw test -Dtest=AuthIntegrationTest                       # single test class
./mvnw test -Dtest=BacktestFlowIntegrationTest#methodName    # single test method
./mvnw package                          # build the jar
```
Default profile uses file-based H2 at `backend/data/h2/`. The `postgres` profile (`SPRING_PROFILES_ACTIVE=postgres`, see [application-postgres.properties](backend/src/main/resources/application-postgres.properties)) is used by docker-compose.

### Frontend (Node 20 — note: system default may be Node 14)
```sh
scripts/dev-frontend.sh                 # dev server with the right Node on PATH (preferred locally)
# or, with Node 20 already active:
cd frontend && npm install && npm run dev   # http://localhost:5173, proxies /api to :8080
npm run build                           # tsc -b && vite build (this is the typecheck gate; there is no test suite)
npm run lint                            # eslint
```

### Full stack via Docker
```sh
docker compose up --build               # UI :3000, API :8080, Postgres
```

## Architecture notes that span multiple files

**Job lifecycle (the core flow).** `POST /api/backtests` validates → persists a `BacktestJob` (`QUEUED`) → returns `202` with the id → a bounded `ThreadPoolTaskExecutor` (4 workers, queue cap 100; full ⇒ `429`) runs it. `BacktestWorker` exports the symbol's CSV cache from the DB if missing, writes `config.json` to a per-job work dir, and `EngineRunner` ([EngineRunner.java](backend/src/main/java/com/samueltan/backtest/engine/EngineRunner.java)) spawns the engine with a 60s hard timeout. Exit 0 ⇒ parse `results.json`, headline metrics into columns + full doc into a JSON column, state `COMPLETED`. Non-zero/timeout ⇒ engine's stderr `{"error": ...}` becomes `errorMessage`, state `FAILED`. The frontend polls `GET /api/backtests/{id}` at ~1s until terminal. On startup, jobs left `QUEUED`/`RUNNING` are marked `FAILED` because the queue is in-memory.

**The engine is the single source of truth for strategies.** `--list-strategies` emits parameter schemas (name, type, default, min/max). The backend (`strategy/`) caches it and serves `GET /api/strategies`; the frontend renders its form from it and the backend validates submissions against it. **Adding a strategy is a C++-only change** (register it in [strategies.cpp](engine/src/strategies.cpp) / [strategy.hpp](engine/include/bt/strategy.hpp)) — it then appears in the UI and server-side validation with zero Java/TS edits. Do not hardcode strategy lists in the backend or frontend.

**Engine internals.** Single-threaded FIFO event loop per bar: fills-first (orders from the previous bar fill at this bar's *open*), then `MarketEvent → Strategy → SignalEvent → Portfolio → OrderEvent → ExecutionHandler`, then mark-to-market. The T→T+1 fill delay and chronological-only bar access are structural lookahead-bias prevention — there's a regression test pinning it; don't "optimize" by letting a strategy see the current close fill. Dependencies (nlohmann/json, Catch2) are vendored in `engine/third_party/`; a C++20 compiler + CMake are the only requirements.

**Data ownership split.** The DB is the source of truth for OHLCV bars (one table, unique `(symbol, date)`). The engine stays DB-free and reads a per-symbol CSV cache the backend exports on demand and invalidates on refresh (`POST /api/market-data/{symbol}/refresh`, pulls from Yahoo Finance). Bundled seed data: `data/csv/` (21 tickers, ~15y), loaded on first start. `scripts/fetch_data.py` regenerates the seed CSVs.

**Auth.** Stateless HS256 JWT carrying the user id; `JwtAuthFilter` puts it in the security context. Strategies/tickers are public GETs; backtests and market-data refresh require a token. Ownership is enforced in queries (`findByIdAndUserId`) — another user's job id returns `404`, not `403`. Override `jwt.secret` via `JWT_SECRET` in prod.

## Runtime data is not tracked

`backend/data/` (H2 DB, engine CSV cache), `engine/build/`, `frontend/dist/`, and `node_modules/` are gitignored build/runtime artifacts. Don't commit them.
