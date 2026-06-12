export const pct = (v: number, digits = 1) => `${(v * 100).toFixed(digits)}%`

export const num = (v: number, digits = 2) => v.toFixed(digits)

export const money = (v: number) =>
  v.toLocaleString('en-US', { style: 'currency', currency: 'USD', maximumFractionDigits: 0 })

/** Profit factor: the engine encodes "infinite" (no losing trades) as -1. */
export const profitFactor = (v: number) => (v < 0 ? '∞' : v.toFixed(2))
