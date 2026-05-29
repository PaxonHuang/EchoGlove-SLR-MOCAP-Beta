// ── SignSubtitle: Card-style highlighted subtitle overlay ──
// Shows the current sign name prominently near the 3D animation.
// Appears with a scale+fade animation when a sign starts playing,
// disappears when idle.

import { useDemoStore } from '../../stores/useDemoStore';
import { SIGN_MAP, CATEGORY_COLORS } from '../../utils/signLanguage';

export default function SignSubtitle() {
  const mode = useDemoStore(s => s.mode);
  const currentSignId = useDemoStore(s => s.currentSignId);
  const queueIndex = useDemoStore(s => s.queueIndex);
  const queue = useDemoStore(s => s.queue);

  const isPlaying = mode !== 'idle';
  const sign = currentSignId ? SIGN_MAP.get(currentSignId) : null;

  if (!isPlaying || !sign) return null;

  const catColor = CATEGORY_COLORS[sign.category] ?? CATEGORY_COLORS['日常基础'];

  return (
    <div className="sign-subtitle-container">
      <div className="sign-subtitle-card">
        {/* Main sign character */}
        <span className="sign-subtitle-char">
          {sign.name}
        </span>

        {/* English name */}
        <span className="sign-subtitle-en">
          {sign.nameEn}
        </span>

        {/* Category badge */}
        <span className={`sign-subtitle-cat ${catColor.bg} ${catColor.text} ${catColor.border} border`}>
          {sign.category}
        </span>

        {/* Sequence progress */}
        {mode === 'sequence' && (
          <span className="sign-subtitle-progress">
            {queueIndex + 1}/{queue.length}
          </span>
        )}
      </div>

      {/* Description line */}
      <p className="sign-subtitle-desc">
        {sign.description}
      </p>
    </div>
  );
}