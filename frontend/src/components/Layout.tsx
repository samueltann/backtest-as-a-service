import { Link, NavLink, Outlet, useNavigate } from 'react-router-dom'
import { useAuth } from '../auth/AuthContext'

export default function Layout() {
  const { email, logout } = useAuth()
  const navigate = useNavigate()

  const navClass = ({ isActive }: { isActive: boolean }) =>
    `px-3 py-2 rounded-md text-sm font-medium ${
      isActive ? 'bg-slate-800 text-emerald-400' : 'text-slate-300 hover:bg-slate-800/60'
    }`

  return (
    <div className="min-h-screen">
      <nav className="border-b border-slate-800 bg-slate-900/80 backdrop-blur">
        <div className="mx-auto flex max-w-6xl items-center justify-between px-4 py-3">
          <div className="flex items-center gap-6">
            <Link to="/" className="text-lg font-semibold tracking-tight text-white">
              backtest<span className="text-emerald-400">.service</span>
            </Link>
            <div className="flex gap-1">
              <NavLink to="/" end className={navClass}>
                Dashboard
              </NavLink>
              <NavLink to="/new" className={navClass}>
                New backtest
              </NavLink>
            </div>
          </div>
          <div className="flex items-center gap-3 text-sm">
            <span className="text-slate-400">{email}</span>
            <button
              onClick={() => {
                logout()
                navigate('/login')
              }}
              className="rounded-md border border-slate-700 px-3 py-1.5 text-slate-300 hover:bg-slate-800"
            >
              Log out
            </button>
          </div>
        </div>
      </nav>
      <main className="mx-auto max-w-6xl px-4 py-8">
        <Outlet />
      </main>
    </div>
  )
}
