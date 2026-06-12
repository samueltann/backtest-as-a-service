import { useEffect, useState, type FormEvent } from 'react'
import { useNavigate } from 'react-router-dom'
import { useMutation, useQuery } from '@tanstack/react-query'
import { api } from '../api/client'
import type { Strategy } from '../api/types'

/**
 * The form is generated from the strategy catalog the backend serves, which
 * itself comes from the C++ engine — adding a strategy to the engine makes it
 * appear here with the right parameter fields, no frontend changes needed.
 */
export default function NewBacktestPage() {
  const navigate = useNavigate()
  const strategies = useQuery({ queryKey: ['strategies'], queryFn: api.strategies })
  const tickers = useQuery({ queryKey: ['tickers'], queryFn: api.tickers })

  const [strategyId, setStrategyId] = useState('')
  const [params, setParams] = useState<Record<string, number>>({})
  const [symbol, setSymbol] = useState('AAPL')
  const [startDate, setStartDate] = useState('2015-01-01')
  const [endDate, setEndDate] = useState('2024-12-31')
  const [initialCapital, setInitialCapital] = useState(100000)
  const [commissionBps, setCommissionBps] = useState(5)
  const [slippageBps, setSlippageBps] = useState(2)
  const [error, setError] = useState<string | null>(null)

  const selected: Strategy | undefined = strategies.data?.find((s) => s.id === strategyId)

  // Default the form to the first strategy and its default params.
  useEffect(() => {
    if (!strategyId && strategies.data?.length) {
      selectStrategy(strategies.data[0])
    }
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [strategies.data])

  function selectStrategy(strategy: Strategy) {
    setStrategyId(strategy.id)
    setParams(Object.fromEntries(strategy.params.map((p) => [p.name, p.default])))
  }

  const submit = useMutation({
    mutationFn: () =>
      api.submitBacktest({
        strategyId,
        params,
        symbol,
        startDate,
        endDate,
        initialCapital,
        commissionBps,
        slippageBps,
        positionFraction: 0.95,
      }),
    onSuccess: (job) => navigate(`/backtests/${job.id}`),
    onError: (err) => setError(err instanceof Error ? err.message : 'submission failed'),
  })

  function onSubmit(e: FormEvent) {
    e.preventDefault()
    setError(null)
    submit.mutate()
  }

  const inputClass =
    'w-full rounded-md border border-slate-700 bg-slate-950 px-3 py-2 text-sm focus:border-emerald-500 focus:outline-none'

  return (
    <div className="mx-auto max-w-2xl">
      <h1 className="mb-6 text-xl font-semibold text-white">New backtest</h1>
      <form onSubmit={onSubmit} className="space-y-6">
        <section className="rounded-xl border border-slate-800 bg-slate-900 p-5">
          <h2 className="mb-3 text-sm font-semibold uppercase tracking-wide text-slate-500">
            Strategy
          </h2>
          <div className="mb-4 grid gap-2 sm:grid-cols-2">
            {strategies.data?.map((s) => (
              <button
                type="button"
                key={s.id}
                onClick={() => selectStrategy(s)}
                className={`rounded-lg border p-3 text-left ${
                  strategyId === s.id
                    ? 'border-emerald-500 bg-emerald-500/5'
                    : 'border-slate-700 hover:border-slate-500'
                }`}
              >
                <div className="text-sm font-medium text-white">{s.name}</div>
                <div className="mt-1 text-xs leading-snug text-slate-400">{s.description}</div>
              </button>
            ))}
          </div>

          {selected && selected.params.length > 0 && (
            <div className="grid gap-4 sm:grid-cols-2">
              {selected.params.map((p) => (
                <div key={p.name}>
                  <label className="mb-1 block text-sm text-slate-400">
                    {p.label}{' '}
                    <span className="text-xs text-slate-600">
                      ({p.min}–{p.max})
                    </span>
                  </label>
                  <input
                    type="number"
                    required
                    min={p.min}
                    max={p.max}
                    step={p.type === 'int' ? 1 : 0.1}
                    value={params[p.name] ?? p.default}
                    onChange={(e) =>
                      setParams({ ...params, [p.name]: Number(e.target.value) })
                    }
                    className={inputClass}
                  />
                </div>
              ))}
            </div>
          )}
        </section>

        <section className="rounded-xl border border-slate-800 bg-slate-900 p-5">
          <h2 className="mb-3 text-sm font-semibold uppercase tracking-wide text-slate-500">
            Market & period
          </h2>
          <div className="grid gap-4 sm:grid-cols-3">
            <div>
              <label className="mb-1 block text-sm text-slate-400">Symbol</label>
              <select value={symbol} onChange={(e) => setSymbol(e.target.value)} className={inputClass}>
                {tickers.data?.map((t) => (
                  <option key={t.symbol} value={t.symbol}>
                    {t.symbol}
                  </option>
                ))}
              </select>
            </div>
            <div>
              <label className="mb-1 block text-sm text-slate-400">Start date</label>
              <input type="date" value={startDate} onChange={(e) => setStartDate(e.target.value)} className={inputClass} />
            </div>
            <div>
              <label className="mb-1 block text-sm text-slate-400">End date</label>
              <input type="date" value={endDate} onChange={(e) => setEndDate(e.target.value)} className={inputClass} />
            </div>
          </div>
        </section>

        <section className="rounded-xl border border-slate-800 bg-slate-900 p-5">
          <h2 className="mb-3 text-sm font-semibold uppercase tracking-wide text-slate-500">
            Capital & costs
          </h2>
          <div className="grid gap-4 sm:grid-cols-3">
            <div>
              <label className="mb-1 block text-sm text-slate-400">Initial capital ($)</label>
              <input
                type="number"
                min={1000}
                step={1000}
                value={initialCapital}
                onChange={(e) => setInitialCapital(Number(e.target.value))}
                className={inputClass}
              />
            </div>
            <div>
              <label className="mb-1 block text-sm text-slate-400">Commission (bps)</label>
              <input
                type="number"
                min={0}
                max={100}
                step={0.5}
                value={commissionBps}
                onChange={(e) => setCommissionBps(Number(e.target.value))}
                className={inputClass}
              />
            </div>
            <div>
              <label className="mb-1 block text-sm text-slate-400">Slippage (bps)</label>
              <input
                type="number"
                min={0}
                max={100}
                step={0.5}
                value={slippageBps}
                onChange={(e) => setSlippageBps(Number(e.target.value))}
                className={inputClass}
              />
            </div>
          </div>
        </section>

        {error && <p className="text-sm text-rose-400">{error}</p>}

        <button
          type="submit"
          disabled={submit.isPending || !strategyId}
          className="w-full rounded-md bg-emerald-600 py-2.5 text-sm font-medium text-white hover:bg-emerald-500 disabled:opacity-50"
        >
          {submit.isPending ? 'Submitting…' : 'Run backtest'}
        </button>
      </form>
    </div>
  )
}
