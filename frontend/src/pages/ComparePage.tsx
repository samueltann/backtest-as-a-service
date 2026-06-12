import { useMemo } from 'react'
import { Link, useSearchParams } from 'react-router-dom'
import { useQueries } from '@tanstack/react-query'
import {
  CartesianGrid,
  Line,
  LineChart,
  Legend,
  ResponsiveContainer,
  Tooltip,
  XAxis,
  YAxis,
} from 'recharts'
import { api } from '../api/client'
import type { JobDetail } from '../api/types'
import { num, pct, profitFactor } from '../format'

const COLORS = ['#10b981', '#38bdf8', '#f59e0b', '#e879f9']

/** Overlays up to four completed backtests, normalized to growth of $1. */
export default function ComparePage() {
  const [searchParams] = useSearchParams()
  const ids = (searchParams.get('ids') ?? '').split(',').filter(Boolean).slice(0, 4)

  const queries = useQueries({
    queries: ids.map((id) => ({ queryKey: ['job', id], queryFn: () => api.job(id) })),
  })

  const jobs = queries
    .map((q) => q.data)
    .filter((j): j is JobDetail => !!j?.results)

  const label = (job: JobDetail) =>
    `${job.strategyId} ${job.symbol} (${Object.values(job.params).join('/')})`

  // Merge the equity curves onto one date axis, each normalized by its
  // initial capital so different starting amounts are comparable.
  const series = useMemo(() => {
    const byDate = new Map<string, Record<string, number | string>>()
    jobs.forEach((job, i) => {
      for (const [date, equity] of job.results!.equity_curve) {
        const row = byDate.get(date) ?? { date }
        row[`s${i}`] = equity / job.initialCapital
        byDate.set(date, row)
      }
    })
    return [...byDate.values()].sort((a, b) =>
      String(a.date).localeCompare(String(b.date)),
    )
  }, [jobs])

  if (ids.length < 2) {
    return (
      <p className="text-slate-400">
        Select at least two completed backtests on the{' '}
        <Link to="/" className="text-emerald-400 hover:underline">dashboard</Link> to compare.
      </p>
    )
  }
  if (queries.some((q) => q.isLoading)) return <p className="text-slate-400">Loading…</p>

  return (
    <div className="space-y-6">
      <h1 className="text-xl font-semibold text-white">Compare backtests</h1>

      <div className="rounded-xl border border-slate-800 bg-slate-900 p-5">
        <h2 className="mb-4 text-sm font-semibold uppercase tracking-wide text-slate-500">
          Growth of $1
        </h2>
        <ResponsiveContainer width="100%" height={360}>
          <LineChart data={series} margin={{ top: 8, right: 8, bottom: 0, left: 8 }}>
            <CartesianGrid stroke="#1e293b" vertical={false} />
            <XAxis dataKey="date" tick={{ fill: '#64748b', fontSize: 11 }} minTickGap={60} />
            <YAxis
              tick={{ fill: '#64748b', fontSize: 11 }}
              tickFormatter={(v: number) => `${v.toFixed(1)}x`}
              width={48}
              domain={['auto', 'auto']}
            />
            <Tooltip
              contentStyle={{
                backgroundColor: '#0f172a',
                border: '1px solid #334155',
                borderRadius: 8,
                fontSize: 12,
              }}
              formatter={(value, name) => [
                `${(value as number).toFixed(3)}x`,
                jobs[Number(String(name).slice(1))] ? label(jobs[Number(String(name).slice(1))]) : name,
              ]}
            />
            <Legend
              formatter={(value: string) => {
                const job = jobs[Number(value.slice(1))]
                return <span className="text-xs text-slate-300">{job ? label(job) : value}</span>
              }}
            />
            {jobs.map((_, i) => (
              <Line
                key={i}
                type="monotone"
                dataKey={`s${i}`}
                stroke={COLORS[i]}
                strokeWidth={1.5}
                dot={false}
                connectNulls
              />
            ))}
          </LineChart>
        </ResponsiveContainer>
      </div>

      <div className="overflow-x-auto rounded-xl border border-slate-800">
        <table className="w-full text-left text-sm">
          <thead className="bg-slate-900 text-xs uppercase tracking-wide text-slate-500">
            <tr>
              <th className="px-3 py-3">Metric</th>
              {jobs.map((job, i) => (
                <th key={job.id} className="px-3 py-3" style={{ color: COLORS[i] }}>
                  <Link to={`/backtests/${job.id}`} className="hover:underline">
                    {label(job)}
                  </Link>
                </th>
              ))}
            </tr>
          </thead>
          <tbody className="divide-y divide-slate-800/70 font-mono text-xs">
            {(
              [
                ['Total return', (j) => pct(j.results!.metrics.total_return)],
                ['CAGR', (j) => pct(j.results!.metrics.cagr)],
                ['Sharpe', (j) => num(j.results!.metrics.sharpe)],
                ['Sortino', (j) => num(j.results!.metrics.sortino)],
                ['Max drawdown', (j) => pct(j.results!.metrics.max_drawdown)],
                ['Win rate', (j) => pct(j.results!.metrics.win_rate, 0)],
                ['Profit factor', (j) => profitFactor(j.results!.metrics.profit_factor)],
                ['Trades', (j) => String(j.results!.metrics.num_trades)],
              ] as [string, (j: JobDetail) => string][]
            ).map(([name, render]) => (
              <tr key={name}>
                <td className="px-3 py-2 font-sans text-slate-400">{name}</td>
                {jobs.map((job) => (
                  <td key={job.id} className="px-3 py-2">
                    {render(job)}
                  </td>
                ))}
              </tr>
            ))}
          </tbody>
        </table>
      </div>

      <Link to="/" className="inline-block text-sm text-slate-400 hover:text-emerald-400">
        ← Back to dashboard
      </Link>
    </div>
  )
}
