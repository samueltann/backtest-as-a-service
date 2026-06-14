# Design decisions

Each entry is the one-paragraph answer I'd give in an interview to "why did you do it that way?".

## Process-per-job instead of JNI or a long-lived engine service

The backend spawns the C++ engine as a child process per backtest, passing a `config.json` and reading back a `results.json`. Three reasons. **Fault isolation:** a segfault or hang in C++ kills one job, never the JVM — the runner enforces a hard timeout and surfaces the engine's stderr as the job's error message. **Simplicity of the boundary:** the contract is a JSON file, which is debuggable by hand (`backtest-engine --config job.json`) and language-agnostic. **Evolution story:** the in-process queue + process workers map one-to-one onto a message broker + container workers if this needed to scale horizontally; nothing about the boundary changes. JNI would buy lower latency per call, but a backtest takes ~10 ms of compute against ~50 ms of process overhead — irrelevant for jobs a user polls at 1 Hz — and costs crash-safety and build fragility.

## An event queue inside the engine, even though it's single-threaded

The engine models the system real trading infrastructure uses: `MarketEvent → Strategy → SignalEvent → Portfolio → OrderEvent → ExecutionHandler → FillEvent → Portfolio`, drained from a FIFO queue per bar. For a single-threaded daily-bar backtester a plain loop would compute the same numbers, but the event architecture keeps the four components decoupled (each touches only events, never each other), makes the execution-delay semantics explicit, and is the standard design (it's how you'd structure the live version, where events arrive asynchronously from a feed).

## Lookahead-bias prevention is structural, not a convention

Two rules are enforced by the engine's shape rather than by discipline. (1) The data handler streams bars chronologically and strategies only ever receive the current bar — there is no API to peek ahead. (2) An order created from a signal on bar T fills at bar T+1's **open**, never at T's close: you can't execute at a price that was only knowable when the signal was computed. A regression test pins this: a lookahead-biased engine would produce a measurably different equity curve on the test fixture.

## Orders are sized at the signal close but clamped at fill time

Position size is computed against the signal bar's close, but the fill happens at the next open, which can gap up. Rather than let cash go negative (silently wrong) or reject the order (surprising), the portfolio clamps the quantity to what cash actually covers — a partial fill, which is also the realistic behavior. Found by hand-computing a test fixture, which is the argument for golden-number tests over "looks plausible" assertions.

## The engine is the single source of truth for the strategy catalog

`backtest-engine --list-strategies` emits machine-readable parameter schemas (name, type, default, min/max). The backend caches this and serves it to the frontend, which renders its form from it, and validates submissions against it. Adding a strategy is therefore a C++-only change: it appears in the UI with correct fields and server-side validation with zero Java or TypeScript edits. The alternative — duplicating the schema in three languages — drifts.

## 202 + polling instead of WebSockets

`POST /api/backtests` validates, persists the job, returns `202 Accepted` with the job id, and the client polls. Polling is trivially correct across reconnects and load balancers and needs no extra infrastructure; at a 1–1.5 s interval the latency cost is invisible for jobs that run for under a second to a few seconds. WebSockets/SSE earn their complexity when there's high-frequency progress to stream (e.g. a parameter sweep with per-cell updates) — that's the natural upgrade path, not the starting point.

## Bounded worker pool with explicit backpressure

Backtests run on a fixed pool of 4 workers (each job is a whole OS process, so concurrency is bounded by cores, not threads) over a queue capped at 100. When the queue is full, submission fails fast with `429` rather than accepting unbounded work — memory stays bounded and the client gets an honest signal. Orphan recovery on startup (jobs stuck QUEUED/RUNNING when the server died are marked FAILED) keeps restarts clean because the queue is in-memory by design.

## Database is the source of truth for market data; the engine reads CSV

Bars live in one table with a unique `(symbol, date)` index; the engine deliberately stays dependency-free (no DB driver in C++) and reads a per-symbol CSV cache the backend exports on demand and invalidates on refresh. This splits responsibilities cleanly: the JVM owns data lifecycle, the engine owns computation, and the cache regeneration cost (~50 ms) is paid once per symbol, not per backtest.

## Key metrics are columns; the full result is a JSON blob

Job lists and comparisons need `sharpe`, `total_return`, etc. for every row — those are real columns. The equity curve and trade log (hundreds of KB) are needed only when viewing one result — those stay as a JSON document serialized into a single CLOB/TEXT column (`@Lob String`), not a native `jsonb` type: the app never queries *into* that document, so a portable large-text column works on both H2 (dev) and Postgres (prod) with no dialect-specific mapping. Relational where queried, opaque document where not.

## Stateless JWT auth

Register/login issue an HS256-signed token carrying the user id; every request is authenticated by signature verification alone, so there's no session store and any backend replica can serve any request. BCrypt for password hashing. CSRF protection is disabled deliberately: the token travels in the `Authorization` header (not a cookie), so a cross-site request can't carry it. Ownership is enforced in queries (`findByIdAndUserId`) — another user's job id is indistinguishable from a nonexistent one (404, not 403), which avoids leaking which ids exist.

## H2 by default, Postgres in compose

Local development and tests run on file/in-memory H2 (zero setup, fast CI); `docker compose up` runs the same schema on Postgres via a Spring profile. JPA keeps the SQL portable. The trade-off — H2 isn't byte-for-byte Postgres — is acceptable because nothing here uses Postgres-specific features; the integration tests that matter (engine flow, auth) are DB-agnostic.

## Monorepo

The three components share one contract (the engine's JSON config/results and the strategy catalog). A contract change lands as one atomic commit across engine, backend, and frontend; one CI pipeline and one compose file ship the whole system. Polyrepo's benefits (independent release cadence, per-team ownership) don't exist for a solo project that deploys as a unit.
