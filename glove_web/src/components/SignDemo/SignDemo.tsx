// ── SignDemo: Sign language teaching demo page ──
// 3D-dominant layout: canvas fills the viewport, drawer overlays as collapsible bottom panel.
// Card-style subtitle overlay appears prominently during playback.

import { Suspense, lazy, useCallback, useState } from 'react';
import { useDemoStore } from '../../stores/useDemoStore';
import { SIGN_DEFINITIONS, CATEGORY_COLORS, SIGN_MAP } from '../../utils/signLanguage';
import SignCard from './SignCard';
import PlaybackControls from './PlaybackControls';
import SignSubtitle from './SignSubtitle';

const DemoCanvas = lazy(() =>
  import('./DemoCanvas').then(m => ({ default: m.DemoCanvas })),
);

export default function SignDemo() {
  const playSign = useDemoStore(s => s.playSign);
  const playSequence = useDemoStore(s => s.playSequence);
  const currentSignId = useDemoStore(s => s.currentSignId);
  const mode = useDemoStore(s => s.mode);
  const [drawerOpen, setDrawerOpen] = useState(false);
  const [activeCategory, setActiveCategory] = useState<string | null>(null);

  const handleCardClick = useCallback((signId: string) => {
    const sign = SIGN_MAP.get(signId);
    if (sign) playSign(sign.id, sign.keyframes, sign.wave);
  }, [playSign]);

  const handlePlaySequence = useCallback(() => {
    const signs = SIGN_DEFINITIONS.map(s => ({
      signId: s.id,
      keyframes: s.keyframes,
      wave: s.wave,
    }));
    playSequence(signs);
  }, [playSequence]);

  const categories = SIGN_DEFINITIONS.reduce((acc, sign) => {
    if (!acc.includes(sign.category)) acc.push(sign.category);
    return acc;
  }, [] as string[]);

  const isPlaying = mode !== 'idle';

  return (
    <div className="relative h-full overflow-hidden">
      {/* ── 3D Canvas — full viewport, dominant area ── */}
      <div className="absolute inset-0">
        <Suspense
          fallback={
            <div className="flex h-full items-center justify-center bg-slate-900">
              <div className="flex flex-col items-center gap-3">
                <div className="h-8 w-8 animate-spin rounded-full border-2 border-blue-500 border-t-transparent" />
                <span className="text-sm text-slate-400">加载 3D 手部模型...</span>
              </div>
            </div>
          }
        >
          <DemoCanvas />
        </Suspense>

        {/* ── Subtitle overlay — prominent, top-center ── */}
        <SignSubtitle />

        {/* ── Toggle drawer button (top-right) ── */}
        <button
          onClick={() => setDrawerOpen(!drawerOpen)}
          className="absolute top-3 right-3 flex items-center gap-2 rounded-lg border border-slate-600/60 bg-slate-800/80 px-3 py-2 text-sm text-slate-300 backdrop-blur-sm hover:bg-slate-700/80 hover:text-white transition-all duration-200 z-20"
        >
          <svg className="h-4 w-4" fill="none" viewBox="0 0 24 24" stroke="currentColor" strokeWidth={2}>
            <path strokeLinecap="round" strokeLinejoin="round" d={drawerOpen ? 'M6 18L18 6M6 6l12 12' : 'M4 6h16M4 12h16M4 18h16'} />
          </svg>
          {drawerOpen ? '关闭' : '手势列表'}
        </button>

        {/* ── Floating stop button when drawer closed & playing ── */}
        {!drawerOpen && isPlaying && (
          <div className="absolute bottom-4 left-1/2 -translate-x-1/2 flex items-center gap-2 z-20">
            <button
              onClick={() => useDemoStore.getState().stop()}
              className="flex items-center gap-1.5 rounded-lg bg-red-600/80 px-4 py-2.5 text-sm font-medium text-white backdrop-blur-sm hover:bg-red-500 transition-all duration-150 shadow-lg"
            >
              <svg className="h-3.5 w-3.5" fill="none" viewBox="0 0 24 24" stroke="currentColor" strokeWidth={2}>
                <path strokeLinecap="round" strokeLinejoin="round" d="M6 18L18 6M6 6l12 12" />
              </svg>
              停止
            </button>
          </div>
        )}

        {/* ── Finger color legend (compact, bottom-left) ── */}
        <div className="absolute bottom-3 left-3 flex gap-2 pointer-events-none z-10">
          {[
            { color: '#ef4444', label: '拇指' },
            { color: '#f59e0b', label: '食指' },
            { color: '#22c55e', label: '中指' },
            { color: '#3b82f6', label: '无名指' },
            { color: '#a855f7', label: '小指' },
          ].map(f => (
            <div key={f.label} className="flex items-center gap-1">
              <span className="block h-1.5 w-1.5 rounded-full" style={{ background: f.color }} />
              <span className="text-[10px] text-slate-500">{f.label}</span>
            </div>
          ))}
        </div>
      </div>

      {/* ── Bottom drawer (overlays on top of canvas) ── */}
      {drawerOpen && (
        <div className="absolute bottom-0 left-0 right-0 z-30 demo-drawer">
          {/* Playback controls */}
          <div className="px-3 pt-2 pb-1">
            <PlaybackControls onPlaySequence={handlePlaySequence} />
          </div>

          {/* Category filter tabs */}
          <div className="flex gap-2 px-3 py-1.5">
            <button
              onClick={() => setActiveCategory(null)}
              className={`rounded-md px-2.5 py-1 text-xs font-medium transition-all duration-150 ${
                activeCategory === null
                  ? 'bg-slate-600/80 text-white'
                  : 'bg-slate-800/50 text-slate-400 hover:text-slate-300'
              }`}
            >
              全部
            </button>
            {categories.map(cat => {
              const cc = CATEGORY_COLORS[cat] ?? CATEGORY_COLORS['日常基础'];
              return (
                <button
                  key={cat}
                  onClick={() => setActiveCategory(cat)}
                  className={`rounded-md px-2.5 py-1 text-xs font-medium transition-all duration-150 ${
                    activeCategory === cat
                      ? `${cc.bg} ${cc.text} border ${cc.border}`
                      : 'bg-slate-800/50 text-slate-400 hover:text-slate-300'
                  }`}
                >
                  {cat}
                </button>
              );
            })}
          </div>

          {/* Sign cards — compact horizontal scroll */}
          <div className="px-3 pb-3 overflow-x-auto">
            <div className="flex gap-2">
              {SIGN_DEFINITIONS
                .filter(s => activeCategory === null || s.category === activeCategory)
                .map(sign => (
                  <SignCard
                    key={sign.id}
                    sign={sign}
                    isActive={currentSignId === sign.id}
                    onClick={() => handleCardClick(sign.id)}
                  />
                ))}
            </div>
          </div>
        </div>
      )}
    </div>
  );
}