import type { JobStatus } from '../api/types'

const styles: Record<JobStatus, string> = {
  QUEUED: 'bg-slate-700/50 text-slate-300',
  RUNNING: 'bg-sky-500/15 text-sky-400',
  COMPLETED: 'bg-emerald-500/15 text-emerald-400',
  FAILED: 'bg-rose-500/15 text-rose-400',
}

export default function StatusBadge({ status }: { status: JobStatus }) {
  return (
    <span className={`inline-flex items-center gap-1.5 rounded-full px-2.5 py-0.5 text-xs font-medium ${styles[status]}`}>
      {status === 'RUNNING' && (
        <span className="h-1.5 w-1.5 animate-pulse rounded-full bg-sky-400" />
      )}
      {status}
    </span>
  )
}
