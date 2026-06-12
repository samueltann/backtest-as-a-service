// Mirrors the backend DTOs (JobDtos.java) and the engine's results document.

export type JobStatus = 'QUEUED' | 'RUNNING' | 'COMPLETED' | 'FAILED'

export interface StrategyParam {
  name: string
  label: string
  type: 'int' | 'number'
  default: number
  min: number
  max: number
}

export interface Strategy {
  id: string
  name: string
  description: string
  params: StrategyParam[]
}

export interface Ticker {
  symbol: string
  minDate: string
  maxDate: string
  barCount: number
}

export interface Metrics {
  total_return: number
  cagr: number
  sharpe: number
  sortino: number
  max_drawdown: number
  max_drawdown_days: number
  win_rate: number
  profit_factor: number
  num_trades: number
  final_equity: number
}

export interface Trade {
  entry_date: string
  exit_date: string
  quantity: number
  entry_price: number
  exit_price: number
  pnl: number
  return_pct: number
}

export interface BacktestResults {
  symbol: string
  strategy: { id: string; params: Record<string, number> }
  metrics: Metrics
  equity_curve: [string, number][]
  trades: Trade[]
}

export interface MetricsSummary {
  totalReturn: number
  cagr: number
  sharpe: number
  sortino: number
  maxDrawdown: number
  maxDrawdownDays: number
  winRate: number
  profitFactor: number
  numTrades: number
  finalEquity: number
}

export interface JobSummary {
  id: string
  strategyId: string
  params: Record<string, number>
  symbol: string
  startDate: string
  endDate: string
  status: JobStatus
  errorMessage: string | null
  createdAt: string
  metrics: MetricsSummary | null
}

export interface JobDetail {
  id: string
  strategyId: string
  params: Record<string, number>
  symbol: string
  startDate: string
  endDate: string
  initialCapital: number
  commissionBps: number
  slippageBps: number
  positionFraction: number
  status: JobStatus
  errorMessage: string | null
  createdAt: string
  startedAt: string | null
  finishedAt: string | null
  results: BacktestResults | null
}

export interface JobPage {
  content: JobSummary[]
  page: { totalElements: number; totalPages: number; number: number; size: number }
}

export interface BacktestRequest {
  strategyId: string
  params: Record<string, number>
  symbol: string
  startDate: string
  endDate: string
  initialCapital: number
  commissionBps: number
  slippageBps: number
  positionFraction: number
}

export interface AuthResult {
  token: string
  email: string
}
