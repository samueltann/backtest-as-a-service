import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest'
import { ApiError, api, tokenStore } from './client'

function jsonResponse(status: number, body: unknown): Response {
  return new Response(status === 204 ? null : JSON.stringify(body), {
    status,
    headers: { 'Content-Type': 'application/json' },
  })
}

describe('api client', () => {
  beforeEach(() => {
    localStorage.clear()
  })
  afterEach(() => {
    vi.restoreAllMocks()
  })

  it('attaches the bearer token and parses the JSON body', async () => {
    tokenStore.set('tok123', 'me@test.com')
    const fetchMock = vi.fn().mockResolvedValue(jsonResponse(200, [{ symbol: 'AAPL' }]))
    vi.stubGlobal('fetch', fetchMock)

    const tickers = await api.tickers()

    expect(tickers).toEqual([{ symbol: 'AAPL' }])
    const headers = fetchMock.mock.calls[0][1].headers
    expect(headers.Authorization).toBe('Bearer tok123')
  })

  it('throws ApiError carrying the server error message and status', async () => {
    vi.stubGlobal('fetch', vi.fn().mockResolvedValue(jsonResponse(400, { error: 'bad params' })))

    await expect(api.tickers()).rejects.toMatchObject({
      message: 'bad params',
      status: 400,
    })
    await expect(api.tickers()).rejects.toBeInstanceOf(ApiError)
  })

  it('clears the stored token when a request is unauthorized', async () => {
    tokenStore.set('expired', 'me@test.com')
    vi.stubGlobal('fetch', vi.fn().mockResolvedValue(jsonResponse(403, { error: 'forbidden' })))

    await expect(api.jobs()).rejects.toBeInstanceOf(ApiError)
    expect(tokenStore.get()).toBeNull()
  })

  it('returns undefined for 204 No Content (delete)', async () => {
    vi.stubGlobal('fetch', vi.fn().mockResolvedValue(jsonResponse(204, null)))
    await expect(api.deleteJob('some-id')).resolves.toBeUndefined()
  })
})
