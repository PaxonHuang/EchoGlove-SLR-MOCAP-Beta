import { create } from 'zustand';
import type { RelayMessage, V5HandData } from '../types';
import { isV5Message } from '../types';

interface HandSensorData {
  flex: number[];    // 5 values
  euler: number[];   // 3 values
  gyro: number[];    // 3 values
}

interface SensorState {
  // V5 per-hand data
  leftHand: HandSensorData;
  rightHand: HandSensorData;

  // V3 backward compat (mapped from V5)
  hall: number[];
  imu: number[];
  timestamp: number;

  // Streaming state
  isStreaming: boolean;
  activeTier: string;
  blendAlpha: number;

  // Performance metrics
  fps: number;
  packetCount: number;
  lastPacketTime: number;

  // Actions
  updateFromRelay: (data: RelayMessage) => void;
  reset: () => void;
}

const EMPTY_HAND: HandSensorData = {
  flex: new Array(5).fill(0),
  euler: new Array(3).fill(0),
  gyro: new Array(3).fill(0),
};

const INITIAL_STATE = {
  leftHand: { ...EMPTY_HAND, flex: [...EMPTY_HAND.flex], euler: [...EMPTY_HAND.euler], gyro: [...EMPTY_HAND.gyro] },
  rightHand: { ...EMPTY_HAND, flex: [...EMPTY_HAND.flex], euler: [...EMPTY_HAND.euler], gyro: [...EMPTY_HAND.gyro] },
  hall: new Array(15).fill(0) as number[],
  imu: new Array(6).fill(0) as number[],
  timestamp: 0,
  isStreaming: false,
  activeTier: 'tier1',
  blendAlpha: 1.0,
  fps: 0,
  packetCount: 0,
  lastPacketTime: 0,
};

function handDataToArrays(hand: V5HandData): HandSensorData {
  return {
    flex: hand.flex || [0, 0, 0, 0, 0],
    euler: hand.euler || [0, 0, 0],
    gyro: hand.gyro || [0, 0, 0],
  };
}

export const useSensorStore = create<SensorState>((set, get) => ({
  ...INITIAL_STATE,

  updateFromRelay: (data: RelayMessage) => {
    const now = performance.now();
    const prev = get();

    // Calculate FPS
    const newPacketCount = prev.packetCount + 1;
    let fps = prev.fps;
    if (prev.lastPacketTime > 0) {
      const interval = now - prev.lastPacketTime;
      const instantFps = interval > 0 ? 1000 / interval : 0;
      fps = Math.round(fps * 0.7 + instantFps * 0.3);
    }

    if (isV5Message(data)) {
      // V5 dual-hand format
      const left = handDataToArrays(data.left_hand);
      const right = handDataToArrays(data.right_hand);

      // Map to V3 compat: hall = left.flex + right.flex + padding, imu = left.euler + left.gyro
      const hall = [...left.flex, ...right.flex, 0, 0, 0, 0, 0];
      const imu = [...left.euler, ...left.gyro];

      set({
        leftHand: left,
        rightHand: right,
        hall,
        imu,
        timestamp: Date.now(),
        isStreaming: true,
        activeTier: data.tier || 'tier1',
        blendAlpha: data.blend_alpha ?? 1.0,
        fps,
        packetCount: newPacketCount,
        lastPacketTime: now,
      });
    } else {
      // V3 flat format (backward compat)
      const hall = data.hall || [];
      const imu = data.imu || [];
      const flexL = hall.slice(0, 5);
      const flexR = hall.slice(5, 10);
      const euler = imu.slice(0, 3);
      const gyro = imu.slice(3, 6);

      set({
        leftHand: { flex: flexL, euler, gyro },
        rightHand: { flex: flexR, euler: [0, 0, 0], gyro: [0, 0, 0] },
        hall,
        imu,
        timestamp: data.timestamp,
        isStreaming: data.status === 'STREAMING',
        activeTier: 'tier1',
        blendAlpha: 1.0,
        fps,
        packetCount: newPacketCount,
        lastPacketTime: now,
      });
    }
  },

  reset: () => {
    set(INITIAL_STATE);
  },
}));
