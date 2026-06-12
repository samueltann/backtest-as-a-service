import type {
  AuthResult,
  BacktestRequest,
  JobDetail,
  JobPage,
  Strategy,
  Ticker,
} from './types'

const TOKEN_KEY = 'backtest.token'
const EMAIL_KEY = 'backtest.email'

export const tokenStore = {
  get: () => localStorage.getItem(TOKEN_KEY),
  getEmail: () => localStorage.getItem(EMAIL_KEY),
  set: (token: string, email: string) => {
    localStorage.setItem(TOKEN_KEY, token)
    localStorage.setItem(EMAIL_KEY, email)
  },
  clear: () => {
    localStorage.removeItem(TOKEN_KEY)
    localStorage.removeItem(EMAIL_KEY)
  },
}

export class ApiError extends Error {
  readonly status: number

  constructor(message: string, status: number) {
    super(message)
    this.status = status
  }
}

async function request<T>(path: string, options: RequestInit = {}): Promise<T> {
  const headers: Record<string, string> = {
    'Content-Type': 'application/json',
    ...(options.headers as Record<string, string>),
  }
  const token = tokenStore.get()
  if (token) headers.Authorization = `Bearer ${token}`

  const response = await fetch(path, { ...options, headers })
  if (!response.ok) {
    let message = `request failed (${response.status})`
    try {
      const body = await response.json()
      if (body.error) message = body.error
    } catch {
      /* non-JSON error body */
    }
    if (response.status === 401 || response.status === 403) tokenStore.clear()
    throw new ApiError(message, response.status)
  }
  if (response.status === 204) return undefined as T
  return response.json()
}

export const api = {
  register: (email: string, password: string) =>
    request<AuthResult>('/api/auth/register', {
      method: 'POST',
      body: JSON.stringify({ email, password }),
    }),
  login: (email: string, password: string) =>
    request<AuthResult>('/api/auth/login', {
      method: 'POST',
      body: JSON.stringify({ email, password }),
    }),

  strategies: () => request<Strategy[]>('/api/strategies'),
  tickers: () => request<Ticker[]>('/api/tickers'),

  submitBacktest: (body: BacktestRequest) =>
    request<JobDetail>('/api/backtests', { method: 'POST', body: JSON.stringify(body) }),
  job: (id: string) => request<JobDetail>(`/api/backtests/${id}`),
  jobs: (page = 0, size = 50) => request<JobPage>(`/api/backtests?page=${page}&size=${size}`),
  deleteJob: (id: string) =>
    request<void>(`/api/backtests/${id}`, { method: 'DELETE' }),
}
