// ── Sign Language Definitions (Joint Angle Based) ──
// 3 demonstration signs: 你, 好, 再见
// Joint angles in degrees, wrist orientation as quaternion [w,x,y,z].
//
// Joint angle indices (15 total):
//   0: thumb_CMC_flexion, 1: thumb_MCP, 2: thumb_IP
//   3: index_MCP, 4: index_PIP, 5: index_DIP
//   6: middle_MCP, 7: middle_PIP, 8: middle_DIP
//   9: ring_MCP, 10: ring_PIP, 11: ring_DIP
//   12: pinky_MCP, 13: pinky_PIP, 14: pinky_DIP
//
// Rest pose (all angles = 0): fingers pointing +Y, palm facing +Z, thumb +X.
// Flexion axis: local X-axis (positive = curl toward palm).
// Thumb CMC: also X-axis flexion (abduction handled by geometry offset).

import { eulerToQuaternion } from './quaternion';
import type { Quat } from './quaternion';

// ── Types ──

export interface AngleKeyframe {
  jointAngles: number[];  // 15 joint angles in degrees
  wristQuat: Quat;        // wrist orientation [w, x, y, z]
  wristPos?: [number, number, number]; // optional wrist position offset
  duration: number;       // ms to reach this pose from previous
  easing: EasingType;     // easing function for transition
}

export interface SignDefinition {
  id: string;
  name: string;         // Chinese name
  nameEn: string;       // English name
  category: '日常基础' | '情绪表达' | '日常动作';
  description: string;  // short description
  keyframes: AngleKeyframe[];
  wave?: {              // optional wave animation (for goodbye)
    amplitude: number;  // degrees
    frequency: number;  // Hz
    duration: number;   // ms
    axis: 'y' | 'x';   // rotation axis for wave
  };
}

export type EasingType = 'linear' | 'easeInOut' | 'easeOut' | 'easeIn';

// ── Constants ──

// Rest pose: all joints at 0 degrees (fingers extended, pointing up)
export const REST_ANGLES: number[] = new Array(15).fill(0);

// Rest pose wrist quaternion (identity - no rotation)
export const REST_WRIST_QUAT: Quat = [1, 0, 0, 0];

// ── Helper: create angle array ──
function makeAngles(config: {
  thumb?: [number, number, number];   // [CMC, MCP, IP]
  index?: [number, number, number];   // [MCP, PIP, DIP]
  middle?: [number, number, number];
  ring?: [number, number, number];
  pinky?: [number, number, number];
}): number[] {
  const angles = new Array(15).fill(0);
  if (config.thumb) { angles[0] = config.thumb[0]; angles[1] = config.thumb[1]; angles[2] = config.thumb[2]; }
  if (config.index) { angles[3] = config.index[0]; angles[4] = config.index[1]; angles[5] = config.index[2]; }
  if (config.middle) { angles[6] = config.middle[0]; angles[7] = config.middle[1]; angles[8] = config.middle[2]; }
  if (config.ring) { angles[9] = config.ring[0]; angles[10] = config.ring[1]; angles[11] = config.ring[2]; }
  if (config.pinky) { angles[12] = config.pinky[0]; angles[13] = config.pinky[1]; angles[14] = config.pinky[2]; }
  return angles;
}

// ── Helper: quaternion from euler (convenience) ──
const quat = (rollDeg: number, pitchDeg: number, yawDeg: number): Quat =>
  eulerToQuaternion(
    (rollDeg * Math.PI) / 180,
    (pitchDeg * Math.PI) / 180,
    (yawDeg * Math.PI) / 180,
  );

// ── Sign Definitions ──

export const SIGN_DEFINITIONS: SignDefinition[] = [
  // ─── 1. 你 (You) ───
  // Index finger pointing forward, other fingers curled, palm faces left.
  // Wrist: yaw=90° (fingers forward +Z), then roll=-90° (palm left)
  {
    id: 'you',
    name: '你',
    nameEn: 'You',
    category: '日常基础',
    description: '食指直立指向对方，手背与地面呈45°，指尖指向对方',
    keyframes: [
      {
        jointAngles: makeAngles({
          thumb: [45, 30, 20],
          index: [0, 0, 0],
          middle: [90, 100, 90],
          ring: [90, 100, 90],
          pinky: [90, 100, 90],
        }),
        wristQuat: quat(-90, 0, 90),  // palm left, index forward
        wristPos: [0, 0, 0.05],
        duration: 400,
        easing: 'easeOut',
      },
    ],
  },

  // ─── 2. 好 (Good) ───
  // Thumb straight up, all other fingers curled.
  // Wrist: yaw=-90° → Rz(-90°) rotates thumb(+X)→+Y(up), palm(+Z)→-Z(back)
  {
    id: 'good',
    name: '好',
    nameEn: 'Good',
    category: '日常基础',
    description: '拇指直立朝上，其余四指自然弯曲收拢，掌心朝后',
    keyframes: [
      {
        jointAngles: makeAngles({
          thumb: [0, 0, 0],
          index: [90, 100, 90],
          middle: [90, 100, 90],
          ring: [90, 100, 90],
          pinky: [90, 100, 90],
        }),
        wristQuat: quat(0, 0, -90),  // Rz(-90°): thumb up, palm back
        wristPos: [0, 0.03, 0],
        duration: 400,
        easing: 'easeOut',
      },
    ],
  },

  // ─── 3. 再见 (Goodbye) ───
  // Open hand waving, palm forward, thumb up, pinky down.
  // Wrist: roll=90° → Rx(90°) rotates thumb(+X)→+Z(forward), palm(+Z)→+Y...
  // Combined: Rx(90°)*Rz(90°) gives thumb up, palm forward, pinky down
  {
    id: 'goodbye',
    name: '再见',
    nameEn: 'Goodbye',
    category: '日常基础',
    description: '掌心朝外，拇指朝上，腕部左右摆动，表示告别',
    keyframes: [
      {
        jointAngles: makeAngles({
          thumb: [0, 0, 0],
          index: [90, 100, 90],
          middle: [90, 100, 90],
          ring: [90, 100, 90],
          pinky: [0, 0, 0],
        }),
        wristQuat: quat(90, 0, 0),  // Rx(90°): thumb up, palm forward, pinky down
        wristPos: [0, 0.1, 0],
        duration: 400,
        easing: 'easeOut',
      },
    ],
    wave: {
      amplitude: 25,   // ±25 degrees
      frequency: 2,    // 2 Hz
      duration: 1500,  // 1.5 seconds
      axis: 'y',
    },
  },

  // ─── 4. 快乐 (Happy) ───
  // Both hands: fingers slightly curled, palms up, bouncing motion.
  // Wrist: pitch=-30° (tilted up), slight curl on all fingers.
  {
    id: 'happy',
    name: '快乐',
    nameEn: 'Happy',
    category: '情绪表达',
    description: '双手手指微曲，掌心朝上，胸前上下摆动表达喜悦',
    keyframes: [
      {
        jointAngles: makeAngles({
          thumb: [20, 15, 10],
          index: [30, 25, 15],
          middle: [30, 25, 15],
          ring: [30, 25, 15],
          pinky: [30, 25, 15],
        }),
        wristQuat: quat(0, -30, 0),  // palm up, tilted
        wristPos: [0, 0.05, 0.05],
        duration: 350,
        easing: 'easeOut',
      },
    ],
    wave: {
      amplitude: 15,
      frequency: 3,
      duration: 1200,
      axis: 'x',
    },
  },

  // ─── 5. 后悔 (Regret) ───
  // Flat hand on chest, fingers together pointing inward.
  // Wrist: yaw=90° + roll=-90° to place palm on chest area.
  {
    id: 'regret',
    name: '后悔',
    nameEn: 'Regret',
    category: '情绪表达',
    description: '手掌平放于胸口，手指并拢朝内，表达懊悔之意',
    keyframes: [
      {
        jointAngles: makeAngles({
          thumb: [10, 5, 0],
          index: [0, 0, 0],
          middle: [0, 0, 0],
          ring: [0, 0, 0],
          pinky: [0, 0, 0],
        }),
        wristQuat: quat(-90, 0, 90),  // palm facing chest
        wristPos: [0, 0.15, 0.08],
        duration: 500,
        easing: 'easeInOut',
      },
    ],
  },

  // ─── 6. 吃饭 (Eat) ───
  // Right hand in C-shape (thumb+index open, others curled) scooping to mouth.
  // Two keyframes: scooping pose → near mouth.
  {
    id: 'eat',
    name: '吃饭',
    nameEn: 'Eat',
    category: '日常动作',
    description: '右手呈C形，拇食指张开，做舀饭送入口中的动作',
    keyframes: [
      {
        jointAngles: makeAngles({
          thumb: [30, 20, 10],
          index: [20, 15, 10],
          middle: [90, 100, 90],
          ring: [90, 100, 90],
          pinky: [90, 100, 90],
        }),
        wristQuat: quat(0, 0, 0),  // neutral, C-shape
        wristPos: [0, 0.05, 0.1],
        duration: 350,
        easing: 'easeOut',
      },
      {
        jointAngles: makeAngles({
          thumb: [20, 15, 5],
          index: [10, 10, 5],
          middle: [90, 100, 90],
          ring: [90, 100, 90],
          pinky: [90, 100, 90],
        }),
        wristQuat: quat(-20, 0, 10),
        wristPos: [0, 0.2, 0.05],
        duration: 300,
        easing: 'easeInOut',
      },
    ],
  },

  // ─── 7. 睡觉 (Sleep) ───
  // Hands together, palms down, tilted to rest on cheek.
  // Fingers extended, wrist tilted sideways with gentle breathing oscillation.
  {
    id: 'sleep',
    name: '睡觉',
    nameEn: 'Sleep',
    category: '日常动作',
    description: '双手合十侧放于脸旁，手指伸直，模拟入睡姿态',
    keyframes: [
      {
        jointAngles: makeAngles({
          thumb: [10, 5, 0],
          index: [0, 0, 0],
          middle: [0, 0, 0],
          ring: [0, 0, 0],
          pinky: [0, 0, 0],
        }),
        wristQuat: quat(0, 70, -20),  // tilted to side, cheek rest
        wristPos: [0.1, 0.2, 0],
        duration: 600,
        easing: 'easeInOut',
      },
    ],
    wave: {
      amplitude: 5,
      frequency: 0.5,
      duration: 2000,
      axis: 'x',
    },
  },
];

// ── Category Colors ──
export const CATEGORY_COLORS: Record<string, { bg: string; text: string; border: string }> = {
  '日常基础': { bg: 'bg-blue-500/15', text: 'text-blue-400', border: 'border-blue-500/30' },
  '情绪表达': { bg: 'bg-amber-500/15', text: 'text-amber-400', border: 'border-amber-500/30' },
  '日常动作': { bg: 'bg-emerald-500/15', text: 'text-emerald-400', border: 'border-emerald-500/30' },
};

// ── Sign ID to Definition lookup ──
export const SIGN_MAP = new Map(SIGN_DEFINITIONS.map(s => [s.id, s]));

// ── Queue item type ──
export interface QueueItem {
  signId: string;
  keyframes: AngleKeyframe[];
  wave?: SignDefinition['wave'];
}
