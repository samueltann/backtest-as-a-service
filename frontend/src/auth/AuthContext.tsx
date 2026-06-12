import { createContext, useCallback, useContext, useState, type ReactNode } from 'react'
import { api, tokenStore } from '../api/client'

interface AuthState {
  email: string | null
  isAuthenticated: boolean
  login: (email: string, password: string) => Promise<void>
  register: (email: string, password: string) => Promise<void>
  logout: () => void
}

const AuthContext = createContext<AuthState | null>(null)

export function AuthProvider({ children }: { children: ReactNode }) {
  const [email, setEmail] = useState<string | null>(() =>
    tokenStore.get() ? tokenStore.getEmail() : null,
  )

  const login = useCallback(async (userEmail: string, password: string) => {
    const result = await api.login(userEmail, password)
    tokenStore.set(result.token, result.email)
    setEmail(result.email)
  }, [])

  const register = useCallback(async (userEmail: string, password: string) => {
    const result = await api.register(userEmail, password)
    tokenStore.set(result.token, result.email)
    setEmail(result.email)
  }, [])

  const logout = useCallback(() => {
    tokenStore.clear()
    setEmail(null)
  }, [])

  return (
    <AuthContext.Provider value={{ email, isAuthenticated: email !== null, login, register, logout }}>
      {children}
    </AuthContext.Provider>
  )
}

export function useAuth(): AuthState {
  const context = useContext(AuthContext)
  if (!context) throw new Error('useAuth must be used inside AuthProvider')
  return context
}
