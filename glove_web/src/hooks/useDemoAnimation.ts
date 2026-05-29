// ── useDemoAnimation: Angle-based animation engine for sign language demo ──
// Drives 3D hand animation using joint angle interpolation with spring physics.
// Phases: transition-to-target → hold (+ optional wave) → settle-back-to-rest
// Must be used inside R3F Canvas (uses useFrame).

import { useRef } from 'react';
import { useFrame } from '@react-three/fiber';
import { slerpQuaternion } from '../utils/quaternion';
import { useDemoStore } from '../stores/useDemoStore';
import {
  lerpAngles,
  easeInOutQuad,
  easeOutCubic,
  degToRad,
} from '../utils/handKinematics';
import {
  REST_ANGLES,
  REST_WRIST_QUAT,
} from '../utils/signLanguage';
import type { Quat } from '../utils/quaternion';
import type { AngleKeyframe } from '../utils/signLanguage';

// ── Animation phases ──
type Phase = 'idle' | 'transition-to' | 'hold' | 'wave' | 'settle-back';

// ── Hook return type ──
export interface DemoAnimState {
  jointAngles: number[];  // 15 angles in degrees
  wristQuat: Quat;        // [w, x, y, z]
  wristPos: [number, number, number];
  isPlaying: boolean;
  currentSignId: string | null;
}

export function useDemoAnimation(): DemoAnimState {
  // Mutable state refs (updated every frame, not React state)
  const jointAnglesRef = useRef<number[]>([...REST_ANGLES]);
  const wristQuatRef = useRef<Quat>([1, 0, 0, 0]);
  const wristPosRef = useRef<[number, number, number]>([0, 0, 0]);

  // Animation tracking
  const phaseRef = useRef<Phase>('idle');
  const phaseStartRef = useRef(0);
  const currentKeyframeRef = useRef<AngleKeyframe | null>(null);
  const isPlayingRef = useRef(false);
  const signIdRef = useRef<string | null>(null);
  const lastSignIdRef = useRef<string | null>(null);
  const waveStartRef = useRef(0);

  // Settle-back start state (captured when settle-back begins)
  const settleStartAnglesRef = useRef<number[]>([...REST_ANGLES]);
  const settleStartQuatRef = useRef<Quat>([1, 0, 0, 0]);
  const settleStartPosRef = useRef<[number, number, number]>([0, 0, 0]);

  // Store selectors (stable references)
  const storeMode = useDemoStore(s => s.mode);
  const storeSignId = useDemoStore(s => s.currentSignId);
  const storeKeyframes = useDemoStore(s => s.keyframes);
  const advanceQueue = useDemoStore(s => s.advanceQueue);

  // Detect sign change → start transition-to phase
  if (storeSignId !== lastSignIdRef.current) {
    lastSignIdRef.current = storeSignId;
    signIdRef.current = storeSignId;

    if (storeMode !== 'idle' && storeKeyframes.length > 0) {
      currentKeyframeRef.current = storeKeyframes[0];
      phaseRef.current = 'transition-to';
      phaseStartRef.current = 0; // will be set on first frame
      isPlayingRef.current = true;
    } else {
      phaseRef.current = 'settle-back';
      phaseStartRef.current = 0;
      isPlayingRef.current = true;
      settleStartAnglesRef.current = [...jointAnglesRef.current];
      settleStartQuatRef.current = [...wristQuatRef.current] as Quat;
      settleStartPosRef.current = [...wristPosRef.current];
    }
  }

  // Frame loop
  useFrame((_, delta) => {
    void delta; // suppress unused warning
    const now = performance.now();

    // Initialize phase start time
    if (phaseStartRef.current === 0) {
      phaseStartRef.current = now;
    }

    const kf = currentKeyframeRef.current;
    const phase = phaseRef.current;

    if (phase === 'idle') {
      isPlayingRef.current = false;
      return;
    }

    if (phase === 'transition-to' && kf) {
      // Transition from rest pose to target gesture
      const elapsed = now - phaseStartRef.current;
      const rawProgress = Math.min(1, elapsed / kf.duration);
      const easedProgress = kf.easing === 'easeInOut' ? easeInOutQuad(rawProgress)
        : kf.easing === 'easeOut' ? easeOutCubic(rawProgress)
        : rawProgress;

      // Interpolate joint angles
      jointAnglesRef.current = lerpAngles(REST_ANGLES, kf.jointAngles, easedProgress);

      // Interpolate wrist quaternion
      wristQuatRef.current = slerpQuaternion(REST_WRIST_QUAT, kf.wristQuat, easedProgress);

      // Interpolate wrist position
      if (kf.wristPos) {
        wristPosRef.current = [
          kf.wristPos[0] * easedProgress,
          kf.wristPos[1] * easedProgress,
          kf.wristPos[2] * easedProgress,
        ];
      } else {
        wristPosRef.current = [0, 0, 0];
      }

      isPlayingRef.current = true;

      if (rawProgress >= 1) {
        // Transition complete → move to hold phase
        jointAnglesRef.current = [...kf.jointAngles];
        wristQuatRef.current = [...kf.wristQuat] as Quat;
        if (kf.wristPos) wristPosRef.current = [...kf.wristPos];

        // Check if this sign has a wave animation
        const storeState = useDemoStore.getState();
        const currentWave = storeState.wave;

        if (currentWave) {
          phaseRef.current = 'wave';
          waveStartRef.current = now;
          phaseStartRef.current = now;
        } else {
          phaseRef.current = 'hold';
          phaseStartRef.current = now;
        }
      }
    }

    if (phase === 'hold') {
      // Hold the target pose for 600ms, then settle back
      const elapsed = now - phaseStartRef.current;
      const holdDuration = 600;

      if (kf) {
        jointAnglesRef.current = [...kf.jointAngles];
        wristQuatRef.current = [...kf.wristQuat] as Quat;
        if (kf.wristPos) wristPosRef.current = [...kf.wristPos];
      }

      isPlayingRef.current = true;

      if (elapsed >= holdDuration) {
        phaseRef.current = 'settle-back';
        phaseStartRef.current = now;
        settleStartAnglesRef.current = [...jointAnglesRef.current];
        settleStartQuatRef.current = [...wristQuatRef.current] as Quat;
        settleStartPosRef.current = [...wristPosRef.current];
      }
    }

    if (phase === 'wave' && kf) {
      // Wave animation: oscillate wrist rotation
      const waveConfig = useDemoStore.getState().wave;
      if (!waveConfig) {
        phaseRef.current = 'hold';
        phaseStartRef.current = now;
        return;
      }

      const elapsed = now - waveStartRef.current;
      const waveDuration = waveConfig.duration;
      const waveProgress = Math.min(1, elapsed / waveDuration);

      // Oscillation: amplitude * sin(2π * freq * t)
      const waveAngle = waveConfig.amplitude *
        Math.sin(2 * Math.PI * waveConfig.frequency * (elapsed / 1000));
      const waveRad = degToRad(waveAngle);

      // Apply wave as additional rotation around the specified axis
      const waveQuat: Quat = [
        Math.cos(waveRad / 2),
        waveConfig.axis === 'x' ? Math.sin(waveRad / 2) : 0,
        0,
        waveConfig.axis === 'y' ? Math.sin(waveRad / 2) : 0,
      ];

      // Combine base wrist quaternion with wave
      const baseQuat = kf.wristQuat;
      const [bw, bx, by, bz] = baseQuat;
      const [ww, wx, wy, wz] = waveQuat;
      wristQuatRef.current = [
        bw * ww - bx * wx - by * wy - bz * wz,
        bw * wx + bx * ww + by * wz - bz * wy,
        bw * wy - bx * wz + by * ww + bz * wx,
        bw * wz + bx * wy - by * wx + bz * ww,
      ] as Quat;

      jointAnglesRef.current = [...kf.jointAngles];
      if (kf.wristPos) wristPosRef.current = [...kf.wristPos];
      isPlayingRef.current = true;

      // Fade out wave amplitude near the end
      if (waveProgress >= 1) {
        wristQuatRef.current = [...kf.wristQuat] as Quat;
        phaseRef.current = 'hold';
        phaseStartRef.current = now;
      }
    }

    if (phase === 'settle-back') {
      // Smooth return to rest pose from captured start state
      const elapsed = now - phaseStartRef.current;
      const settleDuration = 400; // 400ms settle (matches transition-in)
      const rawProgress = Math.min(1, elapsed / settleDuration);
      const eased = easeOutCubic(rawProgress);

      // Lerp from captured start state to rest pose
      jointAnglesRef.current = lerpAngles(settleStartAnglesRef.current, REST_ANGLES, eased);
      wristQuatRef.current = slerpQuaternion(settleStartQuatRef.current, REST_WRIST_QUAT, eased);
      const sp = settleStartPosRef.current;
      wristPosRef.current = [
        sp[0] + (0 - sp[0]) * eased,
        sp[1] + (0 - sp[1]) * eased,
        sp[2] + (0 - sp[2]) * eased,
      ];

      isPlayingRef.current = true;

      if (rawProgress >= 1) {
        // Settled → advance sequence or go idle
        jointAnglesRef.current = [...REST_ANGLES];
        wristQuatRef.current = [...REST_WRIST_QUAT] as Quat;
        wristPosRef.current = [0, 0, 0];
        phaseRef.current = 'idle';
        isPlayingRef.current = false;

        const storeState = useDemoStore.getState();
        if (storeState.mode === 'sequence') {
          advanceQueue();
        } else {
          storeState.setMode('idle');
        }
      }
    }
  });

  return {
    jointAngles: jointAnglesRef.current,
    wristQuat: wristQuatRef.current,
    wristPos: wristPosRef.current,
    isPlaying: isPlayingRef.current,
    currentSignId: signIdRef.current,
  };
}
