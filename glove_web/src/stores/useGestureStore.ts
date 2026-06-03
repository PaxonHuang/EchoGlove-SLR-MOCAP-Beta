import { create } from 'zustand';
import type { RelayMessage, GestureHistoryEntry } from '../types';
import { isV5Message } from '../types';
import { GESTURE_LABELS } from '../utils/constants';

interface GestureState {
  // V5 per-hand gesture
  leftGestureId: number | null;
  leftConfidence: number | null;
  rightGestureId: number | null;
  rightConfidence: number | null;

  // Fused inference result
  fusedGestureId: number | null;
  fusedConfidence: number | null;
  activeTier: string;
  nlpText: string | null;

  // V3 backward compat
  l1GestureId: number | null;
  l1Confidence: number | null;
  l2GestureId: number | null;
  l2Confidence: number | null;

  // Gesture history
  gestureHistory: GestureHistoryEntry[];

  // Actions
  updateGesture: (data: RelayMessage) => void;
  clearHistory: () => void;
}

const MAX_HISTORY = 10;

function getGestureLabel(id: number | null): string {
  if (id === null || id === undefined || id < 0) return '无手势';
  return GESTURE_LABELS[id] ?? `手势 #${id}`;
}

export const useGestureStore = create<GestureState>((set, get) => ({
  leftGestureId: null,
  leftConfidence: null,
  rightGestureId: null,
  rightConfidence: null,
  fusedGestureId: null,
  fusedConfidence: null,
  activeTier: 'tier1',
  nlpText: null,
  l1GestureId: null,
  l1Confidence: null,
  l2GestureId: null,
  l2Confidence: null,
  gestureHistory: [],

  updateGesture: (data: RelayMessage) => {
    const prev = get();

    if (isV5Message(data)) {
      // V5 dual-hand format
      const leftId = data.left_hand.gesture_id;
      const leftConf = data.left_hand.confidence;
      const rightId = data.right_hand.gesture_id;
      const rightConf = data.right_hand.confidence;
      const fusedId = data.inference.gesture_id;
      const fusedConf = data.inference.confidence;

      const gestureChanged = fusedId !== prev.fusedGestureId;
      const shouldRecord = gestureChanged && fusedId >= 0 && fusedConf > 0.5;

      const newEntry: GestureHistoryEntry | null = shouldRecord
        ? {
            gestureId: fusedId,
            label: getGestureLabel(fusedId),
            confidence: fusedConf,
            nlpText: data.nlp_text,
            timestamp: Date.now(),
            hand: leftId === rightId ? 'both' : (leftConf > rightConf ? 'left' : 'right'),
          }
        : null;

      set({
        leftGestureId: leftId,
        leftConfidence: leftConf,
        rightGestureId: rightId,
        rightConfidence: rightConf,
        fusedGestureId: fusedId,
        fusedConfidence: fusedConf,
        activeTier: data.tier,
        nlpText: data.nlp_text,
        l1GestureId: leftId,
        l1Confidence: leftConf,
        l2GestureId: fusedId,
        l2Confidence: fusedConf,
        gestureHistory: newEntry
          ? [newEntry, ...prev.gestureHistory].slice(0, MAX_HISTORY)
          : prev.gestureHistory,
      });
    } else {
      // V3 flat format
      const gestureChanged = data.l1_gesture_id !== prev.l1GestureId;
      const shouldRecord =
        gestureChanged && data.l1_gesture_id !== null && (data.l1_confidence ?? 0) > 0.5;

      const newEntry: GestureHistoryEntry | null = shouldRecord
        ? {
            gestureId: data.l1_gesture_id,
            label: getGestureLabel(data.l1_gesture_id),
            confidence: data.l1_confidence ?? 0,
            nlpText: data.nlp_text,
            timestamp: data.timestamp,
          }
        : null;

      set({
        leftGestureId: data.l1_gesture_id,
        leftConfidence: data.l1_confidence,
        rightGestureId: null,
        rightConfidence: null,
        fusedGestureId: data.l2_gesture_id,
        fusedConfidence: data.l2_confidence,
        activeTier: 'tier1',
        nlpText: data.nlp_text,
        l1GestureId: data.l1_gesture_id,
        l1Confidence: data.l1_confidence,
        l2GestureId: data.l2_gesture_id,
        l2Confidence: data.l2_confidence,
        gestureHistory: newEntry
          ? [newEntry, ...prev.gestureHistory].slice(0, MAX_HISTORY)
          : prev.gestureHistory,
      });
    }
  },

  clearHistory: () => {
    set({ gestureHistory: [] });
  },
}));

// ── Convenience selectors ──
export const selectLeftLabel = (state: GestureState): string =>
  getGestureLabel(state.leftGestureId);

export const selectRightLabel = (state: GestureState): string =>
  getGestureLabel(state.rightGestureId);

export const selectFusedLabel = (state: GestureState): string =>
  getGestureLabel(state.fusedGestureId);

// V3 compat
export const selectL1Label = (state: GestureState): string =>
  getGestureLabel(state.l1GestureId);

export const selectL2Label = (state: GestureState): string =>
  getGestureLabel(state.l2GestureId);
