// ── Demo Animation Store ──
// Zustand store for sign language demo playback.
// Uses angle-based keyframes (joint angles in degrees).

import { create } from 'zustand';
import type { AngleKeyframe, QueueItem } from '../utils/signLanguage';

export type { QueueItem };

interface DemoState {
  mode: 'idle' | 'single' | 'sequence';
  currentSignId: string | null;
  keyframes: AngleKeyframe[];
  wave: QueueItem['wave'];
  queue: QueueItem[];
  queueIndex: number;

  playSign: (signId: string, keyframes: AngleKeyframe[], wave?: QueueItem['wave']) => void;
  playSequence: (signs: QueueItem[]) => void;
  stop: () => void;
  advanceQueue: () => void;
  setMode: (mode: 'idle' | 'single' | 'sequence') => void;
}

export const useDemoStore = create<DemoState>((set, get) => ({
  mode: 'idle',
  currentSignId: null,
  keyframes: [],
  wave: undefined,
  queue: [],
  queueIndex: 0,

  playSign: (signId, keyframes, wave) => {
    set({
      mode: 'single',
      currentSignId: signId,
      keyframes,
      wave,
      queue: [{ signId, keyframes, wave }],
      queueIndex: 0,
    });
  },

  playSequence: (signs) => {
    if (signs.length === 0) return;
    set({
      mode: 'sequence',
      currentSignId: signs[0].signId,
      keyframes: signs[0].keyframes,
      wave: signs[0].wave,
      queue: signs,
      queueIndex: 0,
    });
  },

  stop: () => {
    set({
      mode: 'idle',
      currentSignId: null,
      keyframes: [],
      wave: undefined,
      queue: [],
      queueIndex: 0,
    });
  },

  advanceQueue: () => {
    const { queue, queueIndex } = get();
    const nextIndex = queueIndex + 1;
    if (nextIndex < queue.length) {
      set({
        currentSignId: queue[nextIndex].signId,
        keyframes: queue[nextIndex].keyframes,
        wave: queue[nextIndex].wave,
        queueIndex: nextIndex,
      });
    } else {
      set({ mode: 'idle' });
    }
  },

  setMode: (mode) => set({ mode }),
}));
