import { useState } from 'react'
import { Link, useNavigate } from 'react-router-dom'
import { useMutation, useQuery, useQueryClient } from '@tanstack/react-query'
import { api } from '../api/client'
import StatusBadge from '../components/StatusBadge'
import { num, pct } from '../format'

export default function DashboardPage() {
  const queryClient = useQueryClient()
  const navigate = useNavigate()
  const [selected, setSelected] = useState<string[]>([])

  // Poll while any job is still in flight so statuses update live.
  const { data, isLoading, error } = useQuery({
    queryKey: ['jobs'],
    queryFn: () => api.jobs(),
    refetchInterval: (query) =>
      query.state.data?.content.some((j) => j.status === 'QUEUED' || j.status === 'RUNNING')
        ? 1500
        : false,
  })

  const deleteJob = useMutation({
    mutationFn: (id: string) => api.deleteJob(id),
    onSuccess: () => queryClient.invalidateQueries({ queryKey: ['jobs'] }),
  })

  function toggleSelect(id: string) {
    setSelected((prev) =>
      prev.includes(id) ? prev.filter((s) => s !== id) : [...prev, id].slice(-4),
    )
  }

  if (isLoading) return <p className="text-slate-400">Loading…</p>
  if (error) return <p className="text-rose-400">{(error as Error).message}</p>

  const jobs = data?.content ?? []

  return (
    <div>
      <div className="mb-6 flex items-center justify-between">
        <h1 className="text-xl font-semibold text-white">Backtests</h1>
        <div className="flex gap-2">
          {selected.length >= 2 && (
            <button
              onClick={() => navigate(`/compare?ids=${selected.join(',')}`)}
              className="rounded-md border border-emerald-700 px-3 py-2 text-sm text-emerald-400 hover:bg-emerald-500/10"
            >
              Compare {selected.length} selected
            </button>
          )}
          <Link
            to="/new"
            className="rounded-md bg-emerald-600 px-3 py-2 text-sm font-medium text-white hover:bg-emerald-500"
          >
            New backtest
          </Link>
        </div>
      </div>

      {jobs.length === 0 ? (
        <div className="rounded-xl border border-dashed border-slate-800 p-12 text-center text-slate-400">
          No backtests yet.{' '}
          <Link to="/new" className="text-emerald-400 hover:underline">
            Run your first one
          </Link>
          .
        </div>
      ) : (
        <div className="overflow-x-auto rounded-xl border border-slate-800">
          <table className="w-full text-left text-sm">
            <thead className="bg-slate-900 text-xs uppercase tracking-wide text-slate-500">
              <tr>
                <th className="px-3 py-3" />
                <th className="px-3 py-3">Strategy</th>
                <th className="px-3 py-3">Symbol</th>
                <th className="px-3 py-3">Period</th>
                <th className="px-3 py-3">Status</th>
                <th className="px-3 py-3 text-right">Return</th>
                <th className="px-3 py-3 text-right">Sharpe</th>
                <th className="px-3 py-3 text-right">Max DD</th>
                <th className="px-3 py-3" />
              </tr>
            </thead>
            <tbody className="divide-y divide-slate-800/70">
              {jobs.map((job) => (
                <tr key={job.id} className="hover:bg-slate-900/50">
                  <td className="px-3 py-3">
                    <input
                      type="checkbox"
                      disabled={job.status !== 'COMPLETED'}
                      checked={selected.includes(job.id)}
                      onChange={() => toggleSelect(job.id)}
                      className="accent-emerald-500 disabled:opacity-30"
                    />
                  </td>
                  <td className="px-3 py-3">
                    <Link to={`/backtests/${job.id}`} className="font-medium text-white hover:text-emerald-400">
                      {job.strategyId}
                    </Link>
                    <div className="text-xs text-slate-500">
                      {Object.entries(job.params)
                        .map(([k, v]) => `${k}=${v}`)
                        .join(' ')}
                    </div>
                  </td>
                  <td className="px-3 py-3 font-mono">{job.symbol}</td>
                  <td className="px-3 py-3 text-slate-400">
                    {job.startDate} → {job.endDate}
                  </td>
                  <td className="px-3 py-3">
                    <StatusBadge status={job.status} />
                    {job.errorMessage && (
                      <div className="mt-1 max-w-xs text-xs text-rose-400">{job.errorMessage}</div>
                    )}
                  </td>
                  <td className={`px-3 py-3 text-right font-mono ${cellColor(job.metrics?.totalReturn)}`}>
                    {job.metrics ? pct(job.metrics.totalReturn) : '—'}
                  </td>
                  <td className="px-3 py-3 text-right font-mono">
                    {job.metrics ? num(job.metrics.sharpe) : '—'}
                  </td>
                  <td className="px-3 py-3 text-right font-mono text-rose-400/80">
                    {job.metrics ? pct(job.metrics.maxDrawdown) : '—'}
                  </td>
                  <td className="px-3 py-3 text-right">
                    <button
                      onClick={() => deleteJob.mutate(job.id)}
                      className="text-xs text-slate-500 hover:text-rose-400"
                    >
                      Delete
                    </button>
                  </td>
                </tr>
              ))}
            </tbody>
          </table>
        </div>
      )}
    </div>
  )
}

function cellColor(value: number | undefined) {
  if (value === undefined) return ''
  return value >= 0 ? 'text-emerald-400' : 'text-rose-400'
}
