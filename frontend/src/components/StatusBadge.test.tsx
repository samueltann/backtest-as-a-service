import { render, screen } from '@testing-library/react'
import { describe, expect, it } from 'vitest'
import StatusBadge from './StatusBadge'

describe('StatusBadge', () => {
  it('shows the status label', () => {
    render(<StatusBadge status="COMPLETED" />)
    expect(screen.getByText('COMPLETED')).toBeInTheDocument()
  })

  it('applies the status-specific colour class', () => {
    render(<StatusBadge status="FAILED" />)
    expect(screen.getByText('FAILED')).toHaveClass('text-rose-400')
  })
})
