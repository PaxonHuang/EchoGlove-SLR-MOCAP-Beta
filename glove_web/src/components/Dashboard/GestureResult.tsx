import { useGestureStore, selectFusedLabel } from '../../stores/useGestureStore';
import { GESTURE_LABELS } from '../../utils/constants';

function ConfidenceBar({ label, confidence, color }: {
  label: string;
  confidence: number | null;
  color: string;
}) {
  const pct = confidence !== null ? Math.round(confidence * 100) : 0;
  return (
    <div className="space-y-1">
      <div className="flex items-center justify-between text-xs">
        <span className="text-slate-400">{label}</span>
        <span className="font-mono text-slate-300">{pct}%</span>
      </div>
      <div className="h-2 w-full overflow-hidden rounded-full bg-slate-800">
        <div
          className={`h-full rounded-full transition-all duration-200 ${color}`}
          style={{ width: `${pct}%` }}
        />
      </div>
    </div>
  );
}

function HandGestureCard({ label, gestureId, confidence, emoji, color }: {
  label: string;
  gestureId: number | null;
  confidence: number | null;
  emoji: string;
  color: string;
}) {
  const displayLabel = gestureId !== null && gestureId >= 0
    ? (GESTURE_LABELS[gestureId] ?? `手势 #${gestureId}`)
    : '无手势';

  return (
    <div className="flex items-center gap-2">
      <div className={`flex h-10 w-10 items-center justify-center rounded-lg ${color} text-lg`}>
        {emoji}
      </div>
      <div className="flex-1 min-w-0">
        <p className="text-xs text-slate-500">{label}</p>
        <p className="text-sm font-bold text-slate-100 truncate">{displayLabel}</p>
      </div>
      <ConfidenceBar label="" confidence={confidence} color="bg-blue-500" />
    </div>
  );
}

export default function GestureResult() {
  const fusedLabel = useGestureStore(selectFusedLabel);
  const leftGestureId = useGestureStore((s) => s.leftGestureId);
  const leftConfidence = useGestureStore((s) => s.leftConfidence);
  const rightGestureId = useGestureStore((s) => s.rightGestureId);
  const rightConfidence = useGestureStore((s) => s.rightConfidence);
  const fusedGestureId = useGestureStore((s) => s.fusedGestureId);
  const fusedConfidence = useGestureStore((s) => s.fusedConfidence);
  const activeTier = useGestureStore((s) => s.activeTier);
  const nlpText = useGestureStore((s) => s.nlpText);
  const history = useGestureStore((s) => s.gestureHistory);

  return (
    <div className="animate-fade-in rounded-lg border border-slate-800 bg-slate-900/50 p-4">
      {/* Fused Result */}
      <div className="mb-4">
        <h3 className="mb-2 text-xs font-semibold uppercase tracking-wider text-slate-500">
          融合识别结果
          <span className="ml-2 text-blue-400">({activeTier})</span>
        </h3>
        <div className="mb-3 flex items-center gap-3">
          <div className="flex h-14 w-14 items-center justify-center rounded-lg bg-blue-500/20 text-2xl">
            ✋
          </div>
          <div className="flex-1">
            <p className="text-lg font-bold text-slate-100">{fusedLabel}</p>
            <p className="text-xs text-slate-400">
              #{fusedGestureId ?? '—'}
            </p>
          </div>
        </div>
        <ConfidenceBar label="融合置信度" confidence={fusedConfidence} color="bg-blue-500" />
      </div>

      {/* Per-Hand Results */}
      <div className="mb-4 space-y-3">
        <h3 className="text-xs font-semibold uppercase tracking-wider text-slate-500">双手识别</h3>
        <HandGestureCard
          label="左手"
          gestureId={leftGestureId}
          confidence={leftConfidence}
          emoji="🤚"
          color="bg-emerald-500/20"
        />
        <HandGestureCard
          label="右手"
          gestureId={rightGestureId}
          confidence={rightConfidence}
          emoji="🤚"
          color="bg-amber-500/20"
        />
      </div>

      {/* NLP Text */}
      {nlpText && (
        <div className="mb-4 rounded-md border border-slate-700 bg-slate-800/50 p-3">
          <p className="mb-1 text-xs text-slate-500">NLP 纠正文本</p>
          <p className="text-sm font-medium text-slate-200">{nlpText}</p>
        </div>
      )}

      {/* Gesture History */}
      <div>
        <div className="mb-2 flex items-center justify-between">
          <h3 className="text-xs font-semibold uppercase tracking-wider text-slate-500">手势历史</h3>
          <span className="text-xs text-slate-600">{history.length} / 10</span>
        </div>
        {history.length === 0 ? (
          <p className="py-4 text-center text-xs text-slate-600">暂无历史记录</p>
        ) : (
          <ul className="max-h-48 space-y-1 overflow-y-auto">
            {history.map((entry, i) => (
              <li
                key={`${entry.timestamp}-${i}`}
                className="flex items-center justify-between rounded px-2 py-1.5 text-xs transition-colors hover:bg-slate-800"
              >
                <div className="flex items-center gap-2">
                  {entry.hand && (
                    <span className="text-[10px] text-slate-600">
                      {entry.hand === 'left' ? 'L' : entry.hand === 'right' ? 'R' : 'B'}
                    </span>
                  )}
                  <span className="font-medium text-slate-300">
                    {GESTURE_LABELS[entry.gestureId ?? 0] ?? entry.label}
                  </span>
                </div>
                <span className="font-mono text-slate-500">
                  {Math.round(entry.confidence * 100)}%
                </span>
              </li>
            ))}
          </ul>
        )}
      </div>
    </div>
  );
}
