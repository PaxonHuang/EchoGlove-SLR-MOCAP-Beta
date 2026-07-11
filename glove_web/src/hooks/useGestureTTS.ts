// ── useGestureTTS: 仪表盘实时识别结果自动播报英文语音 ──
// 监听 fusedGestureId 变化：当融合识别切到新 ID 且置信度≥阈值、且非
// 0/无手势、且与上次播报不同时，speak() 对应英文（GESTURE_LABELS_EN）。
// 防抖：只在"切换到新手势"时触发一次，不随每帧置信度抖动重复播报。

import { useEffect, useRef } from 'react';
import { useGestureStore } from '../stores/useGestureStore';
import { GESTURE_LABELS_EN, TTS_CONFIDENCE_THRESHOLD } from '../utils/constants';
import { useTTS } from './useTTS';

interface Options {
  enabled?: boolean;  // 仅在仪表盘视图激活时启用
}

export function useGestureTTS({ enabled = true }: Options = {}) {
  const { speak, supported } = useTTS();
  const lastSpokenRef = useRef<number | null>(null);

  const fusedGestureId = useGestureStore((s) => s.fusedGestureId);
  const fusedConfidence = useGestureStore((s) => s.fusedConfidence);

  useEffect(() => {
    if (!enabled || !supported) return;
    if (fusedGestureId === null || fusedGestureId <= 0) {
      // 无手势 / idle：重置，使下次重新命中时能再播
      lastSpokenRef.current = null;
      return;
    }
    if (fusedConfidence === null || fusedConfidence < TTS_CONFIDENCE_THRESHOLD) return;
    if (fusedGestureId === lastSpokenRef.current) return;  // 同一手势不重复播

    const label = GESTURE_LABELS_EN[fusedGestureId];
    if (!label) return;

    lastSpokenRef.current = fusedGestureId;
    speak(label, { lang: 'en-US', rate: 0.9 });
  }, [enabled, supported, fusedGestureId, fusedConfidence, speak]);
}
