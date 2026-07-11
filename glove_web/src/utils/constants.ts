// ── 21 Hand Keypoint Names ──
// Layout:  0=WRIST
//          1-4  = THUMB  (CMC, MCP, IP, TIP)
//          5-8  = INDEX  (MCP, PIP, DIP, TIP)
//          9-12 = MIDDLE (MCP, PIP, DIP, TIP)
//          13-16= RING   (MCP, PIP, DIP, TIP)
//          17-20= PINKY  (MCP, PIP, DIP, TIP)
export const FINGER_NAMES: string[] = [
  'wrist',
  'thumb_cmc', 'thumb_mcp', 'thumb_ip', 'thumb_tip',
  'index_mcp', 'index_pip', 'index_dip', 'index_tip',
  'middle_mcp', 'middle_pip', 'middle_dip', 'middle_tip',
  'ring_mcp', 'ring_pip', 'ring_dip', 'ring_tip',
  'pinky_mcp', 'pinky_pip', 'pinky_dip', 'pinky_tip',
];

export const NUM_KEYPOINTS = 21;

// ── 20 Bone Connections: [start_idx, end_idx] ──
export const HAND_CONNECTIONS: [number, number][] = [
  // Thumb (4 bones)
  [0, 1],  // wrist -> thumb_cmc
  [1, 2],  // thumb_cmc -> thumb_mcp
  [2, 3],  // thumb_mcp -> thumb_ip
  [3, 4],  // thumb_ip -> thumb_tip
  // Index (4 bones)
  [0, 5],  // wrist -> index_mcp
  [5, 6],  // index_mcp -> index_pip
  [6, 7],  // index_pip -> index_dip
  [7, 8],  // index_dip -> index_tip
  // Middle (4 bones)
  [0, 9],  // wrist -> middle_mcp
  [9, 10], // middle_mcp -> middle_pip
  [10, 11],// middle_pip -> middle_dip
  [11, 12],// middle_dip -> middle_tip
  // Ring (4 bones)
  [0, 13], // wrist -> ring_mcp
  [13, 14],// ring_mcp -> ring_pip
  [14, 15],// ring_pip -> ring_dip
  [15, 16],// ring_dip -> ring_tip
  // Pinky (4 bones)
  [0, 17], // wrist -> pinky_mcp
  [17, 18],// pinky_mcp -> pinky_pip
  [18, 19],// pinky_pip -> pinky_dip
  [19, 20],// pinky_dip -> pinky_tip
];

export const NUM_BONES = 20;

// ── Finger group indices for coloring ──
export const FINGER_GROUPS: { name: string; indices: number[]; color: string }[] = [
  { name: 'Thumb',  indices: [1, 2, 3, 4],   color: '#ef4444' },
  { name: 'Index',  indices: [5, 6, 7, 8],   color: '#f59e0b' },
  { name: 'Middle', indices: [9, 10, 11, 12], color: '#22c55e' },
  { name: 'Ring',   indices: [13, 14, 15, 16], color: '#3b82f6' },
  { name: 'Pinky',  indices: [17, 18, 19, 20], color: '#a855f7' },
];

// ── Colors ──
export const COLORS = {
  skin: '#f4c28f',
  bone: '#60a5fa',
  joint: '#fbbf24',
  wrist: '#94a3b8',
  background: '#0f172a',
  gridFloor: '#1e293b',
} as const;

// ── V3 Hall Sensor to Finger Mapping (15 sensors, backward compat) ──
export const HALL_FINGER_MAP: number[] = [
  2, 3, 4,       // thumb MCP, IP -> affects thumb
  5, 6, 7,       // index MCP, PIP, DIP
  9, 10, 11,     // middle MCP, PIP, DIP
  13, 14, 15,    // ring MCP, PIP, DIP
  17, 18, 19,    // pinky MCP, PIP, DIP
];

// ── V5 Flex Sensor to Keypoint Mapping (5 sensors per hand) ──
// Each flex sensor maps to the MCP joint of its finger.
// The PIP/DIP joints are derived via 2:1 cascade from the MCP curl.
export const FLEX_FINGER_MAP: { sensorIdx: number; mcpKeypoint: number; pipKeypoint: number; dipKeypoint: number }[] = [
  { sensorIdx: 0, mcpKeypoint: 2,  pipKeypoint: 3,  dipKeypoint: 4  },  // thumb
  { sensorIdx: 1, mcpKeypoint: 5,  pipKeypoint: 6,  dipKeypoint: 7  },  // index
  { sensorIdx: 2, mcpKeypoint: 9,  pipKeypoint: 10, dipKeypoint: 11 },  // middle
  { sensorIdx: 3, mcpKeypoint: 13, pipKeypoint: 14, dipKeypoint: 15 },  // ring
  { sensorIdx: 4, mcpKeypoint: 17, pipKeypoint: 18, dipKeypoint: 19 },  // pinky
];

// ── Gesture Label Map ──
// 0-14: Chinese-gesture labels (legacy V5). 21-26: ASL letters (demo
// classifier). demo_server also sends nlp_text=the letter so the NLP panel
// shows it regardless, but mapping the id here makes the result/history
// panels render the letter directly instead of "手势 #22".
export const GESTURE_LABELS: Record<number, string> = {
  0: '无手势',
  1: '你好',
  2: '谢谢',
  3: '对不起',
  4: '是',
  5: '否',
  6: '请',
  7: '帮',
  8: '爱',
  9: '家',
  10: '学校',
  11: '吃饭',
  12: '喝水',
  13: '好',
  14: '再见',
  // ASL letters (4-channel demo on the broken-ring glove uses A/B/I/L only;
  // W/Y ids retained for completeness but not actively classified).
  21: 'A',
  22: 'B',
  23: 'I',
  24: 'L',
  25: 'W',
  26: 'Y',
};

// ── Defaults ──
export const DEFAULT_RELAY_HOST = 'localhost';
export const DEFAULT_WS_PORT = 8765;
export const MAX_RECONNECT_RETRIES = 10;
export const INITIAL_RECONNECT_DELAY = 1000;
export const LERP_FACTOR = 0.15;
export const FPS_SAMPLE_INTERVAL = 1000;
