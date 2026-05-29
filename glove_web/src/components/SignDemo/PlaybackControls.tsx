// ── PlaybackControls: Compact drawer-style playback bar ──

import { useDemoStore } from '../../stores/useDemoStore';
import { SIGN_DEFINITIONS } from '../../utils/signLanguage';

interface PlaybackControlsProps {
  onPlaySequence: () => void;
}

export default function PlaybackControls({ onPlaySequence }: PlaybackControlsProps) {
  const mode = useDemoStore(s => s.mode);
  const currentSignId = useDemoStore(s => s.currentSignId);
  const queueIndex = useDemoStore(s => s.queueIndex);
  const queue = useDemoStore(s => s.queue);
  const stop = useDemoStore(s => s.stop);

  const isPlaying = mode !== 'idle';
  const currentSign = SIGN_DEFINITIONS.find(s => s.id === currentSignId);

  return (
    <div className="flex items-center gap-2 rounded-lg border border-slate-700/40 bg-slate-800/50 px-3 py-1.5">
      {/* Status */}
      <div className="flex items-center gap-2 min-w-0">
        {isPlaying ? (
          <>
            <span className="relative flex h-2.5 w-2.5 shrink-0">
              <span className="absolute inline-flex h-full w-full animate-ping rounded-full bg-blue-400 opacity-75" />
              <span className="relative inline-flex h-2.5 w-2.5 rounded-full bg-blue-500" />
            </span>
            <span className="text-xs text-slate-300 truncate">
              {currentSign?.name ?? ''}
              {mode === 'sequence' && (
                <span className="text-slate-500 ml-1">
                  ({queueIndex + 1}/{queue.length})
                </span>
              )}
            </span>
          </>
        ) : (
          <span className="text-xs text-slate-500">
            点击卡片播放
          </span>
        )}
      </div>

      {/* Actions */}
      <div className="flex items-center gap-1.5 shrink-0 ml-auto">
        <button
          onClick={onPlaySequence}
          disabled={isPlaying}
          className={`
            flex items-center gap-1 rounded-md px-2.5 py-1.5 text-xs font-medium
            transition-all duration-150
            ${isPlaying
              ? 'bg-slate-700/50 text-slate-500 cursor-not-allowed'
              : 'bg-blue-600 text-white hover:bg-blue-500 active:scale-95'
            }
          `}
        >
          <svg className="h-3 w-3" fill="none" viewBox="0 0 24 24" stroke="currentColor" strokeWidth={2}>
            <path strokeLinecap="round" strokeLinejoin="round" d="M5 3l14 9-14 9V3z" />
          </svg>
          连续演示
        </button>

        {isPlaying && (
          <button
            onClick={stop}
            className="flex items-center gap-1 rounded-md bg-red-600/80 px-2.5 py-1.5 text-xs font-medium text-white hover:bg-red-500 transition-all duration-150 active:scale-95"
          >
            <svg className="h-3 w-3" fill="none" viewBox="0 0 24 24" stroke="currentColor" strokeWidth={2}>
              <path strokeLinecap="round" strokeLinejoin="round" d="M6 18L18 6M6 6l12 12" />
            </svg>
            停止
          </button>
        )}
      </div>
    </div>
  );
}