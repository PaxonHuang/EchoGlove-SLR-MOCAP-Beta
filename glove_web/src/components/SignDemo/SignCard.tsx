// ── SignCard: Compact card for drawer-style horizontal sign selection ──

import type { SignDefinition } from '../../utils/signLanguage';
import { CATEGORY_COLORS } from '../../utils/signLanguage';

interface SignCardProps {
  sign: SignDefinition;
  isActive: boolean;
  onClick: () => void;
}

export default function SignCard({ sign, isActive, onClick }: SignCardProps) {
  const catColor = CATEGORY_COLORS[sign.category] ?? CATEGORY_COLORS['日常基础'];

  return (
    <button
      onClick={onClick}
      className={`
        group flex items-center gap-2 rounded-lg border px-3 py-2
        transition-all duration-200 cursor-pointer shrink-0
        ${isActive
          ? 'border-blue-500/60 bg-blue-500/15 shadow-md shadow-blue-500/10'
          : 'border-slate-700/40 bg-slate-800/40 hover:border-slate-600 hover:bg-slate-700/50'
        }
      `}
    >
      {/* Sign character */}
      <span className={`
        text-2xl font-bold leading-none
        ${isActive ? 'text-blue-300' : 'text-slate-200 group-hover:text-white'}
        transition-colors duration-200
      `}>
        {sign.name}
      </span>

      {/* English name + category */}
      <div className="flex flex-col gap-0.5">
        <span className="text-[11px] text-slate-400 font-medium leading-none">
          {sign.nameEn}
        </span>
        <span className={`text-[10px] font-medium leading-none ${catColor.text}`}>
          {sign.category}
        </span>
      </div>

      {/* Active pulse */}
      {isActive && (
        <span className="ml-1 block h-1.5 w-1.5 rounded-full bg-blue-500 animate-pulse" />
      )}
    </button>
  );
}