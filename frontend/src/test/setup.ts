// Adds jest-dom matchers (toBeInTheDocument, toHaveTextContent, …) to Vitest's
// expect, and clears the DOM between tests.
import '@testing-library/jest-dom/vitest'
import { afterEach } from 'vitest'
import { cleanup } from '@testing-library/react'

afterEach(() => {
  cleanup()
})
