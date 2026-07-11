// ── SignDemo: 手语教学页 ──
// 布局：左 ~60% 3D 画布 + 右 ~40% 滚动按钮面板（CSL 手语 + ASL 字母两组字符按钮）。
// 字符按钮替代原 SignCard 横滚，点击触发对应 keyframes 播放。

import { Suspense, lazy, useCallback } from 'react';
import { useDemoStore } from '../../stores/useDemoStore';
import {
  SIGN_DEFINITIONS,
  SIGN_MAP,
  CATEGORY_COLORS,
} from '../../utils/signLanguage';
import PlaybackControls from './PlaybackControls';
import SignSubtitle from './SignSubtitle';

const DemoCanvas = lazy(() =>
  import('./DemoCanvas').then(m => ({ default: m.DemoCanvas })),
);

// 两组：CSL 手语（非 ASL字母 类别）+ ASL 字母
const CSL_SIGNS = SIGN_DEFINITIONS.filter(s => s.category !== 'ASL字母');
const ASL_SIGNS = SIGN_DEFINITIONS.filter(s => s.category === 'ASL字母');

export default function SignDemo() {
  const playSign = useDemoStore(s => s.playSign);
  const currentSignId = useDemoStore(s => s.currentSignId);
  const mode = useDemoStore(s => s.mode);

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
    useDemoStore.getState().playSequence(signs);
  }, []);

  const isPlaying = mode !== 'idle';

  return (
    <div className="flex h-full overflow-hidden">
      {/* ── 左：3D 画布 ~60% ── */}
      <div className="relative flex-1 min-w-0">
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

        {/* 字幕浮层 */}
        <SignSubtitle />

        {/* 播放中停止按钮 */}
        {isPlaying && (
          <div className="absolute bottom-4 left-1/2 -translate-x-1/2 z-20">
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

        {/* 指色图例 */}
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

      {/* ── 右：按钮面板 ~40% ── */}
      <div className="w-[40%] min-w-[280px] max-w-[420px] border-l border-slate-700/60 bg-slate-900/80 flex flex-col">
        {/* 顶部播放控制 */}
        <div className="px-4 pt-3 pb-2 border-b border-slate-700/40">
          <PlaybackControls onPlaySequence={handlePlaySequence} />
        </div>

        {/* 滚动按钮区 */}
        <div className="flex-1 overflow-y-auto px-4 py-3 space-y-5">
          {/* CSL 手语组 */}
          <ButtonGroup
            title="CSL 手语"
            signs={CSL_SIGNS}
            currentSignId={currentSignId}
            onClick={handleCardClick}
          />
          {/* ASL 字母组 */}
          <ButtonGroup
            title="ASL 字母"
            signs={ASL_SIGNS}
            currentSignId={currentSignId}
            onClick={handleCardClick}
            aslStyle
          />
        </div>
      </div>
    </div>
  );
}

// ── 按钮组 ──
function ButtonGroup({
  title,
  signs,
  currentSignId,
  onClick,
  aslStyle = false,
}: {
  title: string;
  signs: typeof SIGN_DEFINITIONS;
  currentSignId: string | null;
  onClick: (id: string) => void;
  aslStyle?: boolean;
}) {
  return (
    <div>
      <div className="flex items-center gap-2 mb-2">
        <h3 className="text-xs font-semibold text-slate-300 uppercase tracking-wider">{title}</h3>
        <span className="text-[10px] text-slate-600">{signs.length} 项</span>
      </div>
      <div className={aslStyle
        ? "grid grid-cols-3 gap-2"
        : "grid grid-cols-2 gap-2"
      }>
        {signs.map(sign => {
          const isActive = currentSignId === sign.id;
          const cc = CATEGORY_COLORS[sign.category] ?? CATEGORY_COLORS['日常基础'];
          return (
            <button
              key={sign.id}
              onClick={() => onClick(sign.id)}
              className={`rounded-lg border px-3 py-2 text-sm font-medium transition-all duration-150 ${
                isActive
                  ? `${cc.bg} ${cc.text} border ${cc.border} shadow-lg`
                  : 'bg-slate-800/60 text-slate-300 border-slate-700/40 hover:bg-slate-700/60 hover:text-white'
              }`}
            >
              <div className="flex flex-col items-center gap-0.5">
                <span className={aslStyle ? "text-lg leading-none" : "text-sm"}>{sign.name}</span>
                <span className="text-[10px] text-slate-500">{sign.nameEn}</span>
              </div>
            </button>
          );
        })}
      </div>
    </div>
  );
}
