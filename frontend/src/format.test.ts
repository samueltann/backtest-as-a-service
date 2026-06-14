import { describe, expect, it } from 'vitest'
import { money, num, pct, profitFactor } from './format'

describe('format helpers', () => {
  it('pct scales to percent with configurable precision', () => {
    expect(pct(0.2841)).toBe('28.4%')
    expect(pct(0.5, 0)).toBe('50%')
    expect(pct(-0.1)).toBe('-10.0%')
  })

  it('num fixes to the given decimals', () => {
    expect(num(0.83248)).toBe('0.83')
    expect(num(1.5, 0)).toBe('2')
  })

  it('money formats as USD with no cents', () => {
    expect(money(384123.22)).toBe('$384,123')
  })

  it('profitFactor renders the engine infinity sentinel (-1) as ∞', () => {
    expect(profitFactor(2.96)).toBe('2.96')
    expect(profitFactor(-1)).toBe('∞')
  })
})
