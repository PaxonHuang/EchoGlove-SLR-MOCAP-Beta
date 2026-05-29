import { Suspense, lazy, useState } from 'react';
import { useWebSocket } from './hooks/useWebSocket';
import { useSensorStore } from './stores/useSensorStore';
import { useSettingsStore } from './stores/useSettingsStore';
import Header from './components/Layout/Header';
import MobileNav from './components/Layout/MobileNav';
import GestureResult from './components/Dashboard/GestureResult';
import SensorDataPanel from './components/Dashboard/SensorDataPanel';
import StatsPanel from './components/Dashboard/StatsPanel';
import SettingsSidebar from './components/Settings/SettingsSidebar';

const HandCanvas = lazy(() =>
  import('./components/Hand3D/HandCanvas').then((m) => ({ default: m.HandCanvas })),
);

const SignDemo = lazy(() =>
  import('./components/SignDemo/SignDemo').then((m) => ({ default: m.default })),
);

type ViewMode = 'dashboard' | 'demo';

export default function App() {
  const { status } = useWebSocket();
  const fps = useSensorStore((s) => s.fps);
  const isStreaming = useSensorStore((s) => s.isStreaming);
  const { show3D, showDashboard, darkMode } = useSettingsStore();
  const [viewMode, setViewMode] = useState<ViewMode>('dashboard');

  return (
    <div
      className={`flex min-h-screen flex-col ${
        darkMode ? 'dark bg-slate-950 text-slate-100' : 'bg-white text-slate-900'
      }`}
    >
      {/* ── Header ── */}
      <Header connectionStatus={status} fps={fps} />

      {/* ── View Mode Tabs ── */}
      <nav className="flex border-b border-slate-800 bg-slate-950/50">
        {([
          { id: 'dashboard' as const, label: '仪表盘', icon: 'M3 12l2-2m0 0l7-7 7 7M5 10v10a1 1 0 001 1h3m10-11l2 2m-2-2v10a1 1 0 01-1 1h-3m-4 0h4' },
          { id: 'demo' as const, label: '手语教学', icon: 'M7 4V2m0 2a2 2 0 012 2v8a2 2 0 01-2 2m0-12a2 2 0 00-2 2v8a2 2 0 002 2m0 0v2m10-12V2m0 2a2 2 0 012 2v8a2 2 0 01-2 2m0-12a2 2 0 00-2 2v8a2 2 0 002 2m0 0v2' },
        ]).map(tab => (
          <button
            key={tab.id}
            onClick={() => setViewMode(tab.id)}
            className={`
              flex items-center gap-2 px-5 py-2.5 text-sm font-medium transition-all duration-150
              border-b-2 -mb-px
              ${viewMode === tab.id
                ? 'border-blue-500 text-blue-400'
                : 'border-transparent text-slate-500 hover:text-slate-300 hover:border-slate-600'
              }
            `}
          >
            <svg className="h-4 w-4" fill="none" viewBox="0 0 24 24" stroke="currentColor" strokeWidth={1.5}>
              <path strokeLinecap="round" strokeLinejoin="round" d={tab.icon} />
            </svg>
            {tab.label}
          </button>
        ))}
      </nav>

      {/* ── Main Content ── */}
      {viewMode === 'demo' ? (
        <main className="flex flex-1 flex-col overflow-hidden">
          <Suspense
            fallback={
              <div className="flex flex-1 items-center justify-center bg-slate-900">
                <div className="animate-pulse text-slate-400">Loading...</div>
              </div>
            }
          >
            <SignDemo />
          </Suspense>
        </main>
      ) : (
        <main className="flex flex-1 flex-col overflow-hidden lg:flex-row">
          {/* 3D Hand Visualization */}
          {show3D && (
            <section className="relative order-1 min-h-[320px] flex-1 lg:order-none">
              <Suspense
                fallback={
                  <div className="flex h-full items-center justify-center bg-slate-900">
                    <div className="animate-pulse text-slate-400">Loading 3D Hand...</div>
                  </div>
                }
              >
                <HandCanvas />
              </Suspense>
              {!isStreaming && status === 'connected' && (
                <div className="absolute inset-0 flex items-center justify-center bg-slate-950/60">
                  <p className="text-sm text-slate-400">Waiting for sensor data...</p>
                </div>
              )}
            </section>
          )}

          {/* Dashboard Panels */}
          {showDashboard && (
            <aside className="order-2 flex flex-col gap-4 overflow-y-auto border-t border-slate-800 p-4 lg:order-none lg:w-[420px] lg:border-l lg:border-t-0">
              <GestureResult />
              <SensorDataPanel />
              <StatsPanel connectionStatus={status} />
            </aside>
          )}
        </main>
      )}

      {/* ── Mobile Navigation ── */}
      <MobileNav />

      {/* ── Settings Sidebar (rendered via portal or inline) ── */}
      <SettingsSidebar />
    </div>
  );
}
