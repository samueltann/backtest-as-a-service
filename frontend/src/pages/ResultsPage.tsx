import { useMemo } from 'react'
import { Link, useParams } from 'react-router-dom'
import { useQuery } from '@tanstack/react-query'
import {
  Area,
  AreaChart,
  CartesianGrid,
  Line,
  LineChart,
  ResponsiveContainer,
  Tooltip,
  XAxis,
  YAxis,
} from 'recharts'
import { api } from '../api/client'
import StatusBadge from '../components/StatusBadge'
import { money, num, pct, profitFactor } from '../format'

export default function ResultsPage() {
  const { id } = useParams<{ id: string }>()

  // Poll until the job leaves the queue/run states.
  const { data: job, error } = useQuery({
    queryKey: ['job', id],
    queryFn: () => api.job(id!),
    refetchInterval: (query) => {
      const status = query.state.data?.status
      return status === 'QUEUED' || status === 'RUNNING' ? 1000 : false
    },
  })

  const equity = useMemo(
    () =>
      job?.results?.equity_curve.map(([date, value]) => ({ date, equity: value })) ?? [],
    [job],
  )

  // Drawdown series: percentage below the running equity peak.
  const drawdown = useMemo(() => {
    let peak = -Infinity
    return equity.map((point) => {
      peak = Math.max(peak, point.equity)
      return { date: point.date, drawdown: (point.equity / peak - 1) * 100 }
    })
  }, [equity])

  if (error) return <p className="text-rose-400">{(error as Error).message}</p>
  if (!job) return <p className="text-slate-400">Loading…</p>

  return (
    <div className="space-y-6">
      <div className="flex items-center justify-between">
        <div>
          <h1 className="text-xl font-semibold text-white">
            {job.strategyId} <span className="text-slate-500">on</span>{' '}
            <span className="font-mono">{job.symbol}</span>
          </h1>
          <p className="mt-1 text-sm text-slate-400">
            {job.startDate} → {job.endDate} · {money(job.initialCapital)} ·{' '}
            {Object.entries(job.params).map(([k, v]) => `${k}=${v}`).join(', ') || 'no params'} ·{' '}
            {job.commissionBps}bps comm, {job.slippageBps}bps slip
          </p>
        </div>
        <StatusBadge status={job.status} />
      </div>

      {job.status === 'FAILED' && (
        <div className="rounded-xl border border-rose-900 bg-rose-500/5 p-4 text-sm text-rose-300">
          {job.errorMessage}
        </div>
      )}

      {(job.status === 'QUEUED' || job.status === 'RUNNING') && (
        <div className="rounded-xl border border-slate-800 bg-slate-900 p-10 text-center text-slate-400">
          Running on the engine…
        </div>
      )}

      {job.results && (
        <>
          <div className="grid grid-cols-2 gap-3 sm:grid-cols-3 lg:grid-cols-5">
            <Metric label="Total return" value={pct(job.results.metrics.total_return)} highlight={job.results.metrics.total_return >= 0} />
            <Metric label="CAGR" value={pct(job.results.metrics.cagr)} highlight={job.results.metrics.cagr >= 0} />
            <Metric label="Sharpe" value={num(job.results.metrics.sharpe)} />
            <Metric label="Sortino" value={num(job.results.metrics.sortino)} />
            <Metric label="Max drawdown" value={pct(job.results.metrics.max_drawdown)} negative />
            <Metric label="Win rate" value={pct(job.results.metrics.win_rate, 0)} />
            <Metric label="Profit factor" value={profitFactor(job.results.metrics.profit_factor)} />
            <Metric label="Trades" value={String(job.results.metrics.num_trades)} />
            <Metric label="DD duration" value={`${job.results.metrics.max_drawdown_days}d`} />
            <Metric label="Final equity" value={money(job.results.metrics.final_equity)} />
          </div>

          <Panel title="Equity curve">
            <ResponsiveContainer width="100%" height={320}>
              <AreaChart data={equity} margin={{ top: 8, right: 8, bottom: 0, left: 8 }}>
                <defs>
                  <linearGradient id="equityFill" x1="0" y1="0" x2="0" y2="1">
                    <stop offset="0%" stopColor="#10b981" stopOpacity={0.25} />
                    <stop offset="100%" stopColor="#10b981" stopOpacity={0} />
                  </linearGradient>
                </defs>
                <CartesianGrid stroke="#1e293b" vertical={false} />
                <XAxis dataKey="date" tick={{ fill: '#64748b', fontSize: 11 }} minTickGap={60} />
                <YAxis
                  tick={{ fill: '#64748b', fontSize: 11 }}
                  tickFormatter={(v: number) => `$${(v / 1000).toFixed(0)}k`}
                  width={56}
                  domain={['auto', 'auto']}
                />
                <Tooltip
                  contentStyle={tooltipStyle}
                  formatter={(value) => [money(value as number), 'Equity']}
                />
                <Area type="monotone" dataKey="equity" stroke="#10b981" strokeWidth={1.5} fill="url(#equityFill)" />
              </AreaChart>
            </ResponsiveContainer>
          </Panel>

          <Panel title="Drawdown">
            <ResponsiveContainer width="100%" height={180}>
              <LineChart data={drawdown} margin={{ top: 8, right: 8, bottom: 0, left: 8 }}>
                <CartesianGrid stroke="#1e293b" vertical={false} />
                <XAxis dataKey="date" tick={{ fill: '#64748b', fontSize: 11 }} minTickGap={60} />
                <YAxis
                  tick={{ fill: '#64748b', fontSize: 11 }}
                  tickFormatter={(v: number) => `${v.toFixed(0)}%`}
                  width={56}
                />
                <Tooltip
                  contentStyle={tooltipStyle}
                  formatter={(value) => [`${(value as number).toFixed(2)}%`, 'Drawdown']}
                />
                <Line type="monotone" dataKey="drawdown" stroke="#f43f5e" strokeWidth={1.2} dot={false} />
              </LineChart>
            </ResponsiveContainer>
          </Panel>

          <Panel title={`Trades (${job.results.trades.length})`}>
            <div className="max-h-80 overflow-y-auto">
              <table className="w-full text-left text-sm">
                <thead className="sticky top-0 bg-slate-900 text-xs uppercase tracking-wide text-slate-500">
                  <tr>
                    <th className="px-3 py-2">Entry</th>
                    <th className="px-3 py-2">Exit</th>
                    <th className="px-3 py-2 text-right">Qty</th>
                    <th className="px-3 py-2 text-right">Entry px</th>
                    <th className="px-3 py-2 text-right">Exit px</th>
                    <th className="px-3 py-2 text-right">PnL</th>
                    <th className="px-3 py-2 text-right">Return</th>
                  </tr>
                </thead>
                <tbody className="divide-y divide-slate-800/70 font-mono text-xs">
                  {job.results.trades.map((t, i) => (
                    <tr key={i}>
                      <td className="px-3 py-2">{t.entry_date}</td>
                      <td className="px-3 py-2">{t.exit_date}</td>
                      <td className="px-3 py-2 text-right">{t.quantity}</td>
                      <td className="px-3 py-2 text-right">{num(t.entry_price)}</td>
                      <td className="px-3 py-2 text-right">{num(t.exit_price)}</td>
                      <td className={`px-3 py-2 text-right ${t.pnl >= 0 ? 'text-emerald-400' : 'text-rose-400'}`}>
                        {num(t.pnl)}
                      </td>
                      <td className={`px-3 py-2 text-right ${t.return_pct >= 0 ? 'text-emerald-400' : 'text-rose-400'}`}>
                        {pct(t.return_pct)}
                      </td>
                    </tr>
                  ))}
                </tbody>
              </table>
            </div>
          </Panel>
        </>
      )}

      <Link to="/" className="inline-block text-sm text-slate-400 hover:text-emerald-400">
        ← Back to dashboard
      </Link>
    </div>
  )
}

const tooltipStyle = {
  backgroundColor: '#0f172a',
  border: '1px solid #334155',
  borderRadius: 8,
  fontSize: 12,
}

function Metric({
  label,
  value,
  highlight,
  negative,
}: {
  label: string
  value: string
  highlight?: boolean
  negative?: boolean
}) {
  const color = negative
    ? 'text-rose-400'
    : highlight === undefined
      ? 'text-white'
      : highlight
        ? 'text-emerald-400'
        : 'text-rose-400'
  return (
    <div className="rounded-xl border border-slate-800 bg-slate-900 p-4">
      <div className="text-xs uppercase tracking-wide text-slate-500">{label}</div>
      <div className={`mt-1 text-lg font-semibold ${color}`}>{value}</div>
    </div>
  )
}

function Panel({ title, children }: { title: string; children: React.ReactNode }) {
  return (
    <div className="rounded-xl border border-slate-800 bg-slate-900 p-5">
      <h2 className="mb-4 text-sm font-semibold uppercase tracking-wide text-slate-500">{title}</h2>
      {children}
    </div>
  )
}
