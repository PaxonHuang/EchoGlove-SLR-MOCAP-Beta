// ── V5 Dual-Hand Sensor Data ──
export interface V5HandData {
  flex: number[];            // 5 flex sensor values [0,1]
  euler: number[];           // 3 euler angles [roll, pitch, yaw] degrees
  gyro: number[];            // 3 gyro values [x, y, z] deg/s
  gesture_id: number;
  confidence: number;
}

export interface V5RelativeData {
  delta_euler: number[];     // 3 relative euler diffs
  delta_quat_dist: number;   // quaternion distance
  delta_gyro_norm: number;   // gyro magnitude diff
  delta_gyro_axis: number;   // gyro axis diff
}

export interface V5InferenceData {
  gesture_id: number;
  confidence: number;
  text: string;
}

// V5 WebSocket message format
export interface V5SensorMessage {
  tier: 'tier1' | 'tier2' | 'tier3';
  blend_alpha: number;
  left_hand: V5HandData;
  right_hand: V5HandData;
  relative: V5RelativeData;
  inference: V5InferenceData;
  nlp_text: string;
}

// ── V3 Sensor Message (backward compat) ──
export interface SensorMessage {
  timestamp: number;
  hall: number[];          // 15 floats (V3 flex sensor values)
  imu: number[];           // 6 floats [gyro_x, gyro_y, gyro_z, accel_x, accel_y, accel_z]
  l1_gesture_id: number | null;
  l1_confidence: number | null;
  l2_gesture_id: number | null;
  l2_confidence: number | null;
  nlp_text: string | null;
  status: 'STREAMING' | 'MODEL_SWITCHING' | 'ERROR';
}

// Union type for WebSocket messages
export type RelayMessage = V5SensorMessage | SensorMessage;

// Type guard
export function isV5Message(msg: RelayMessage): msg is V5SensorMessage {
  return 'left_hand' in msg && 'right_hand' in msg;
}

// ── 3D Hand Pose ──
export interface Keypoint3D {
  x: number;
  y: number;
  z: number;
}

export interface HandPose {
  keypoints: Keypoint3D[];           // 21 keypoints
  quaternion: [number, number, number, number]; // [w, x, y, z]
  handedness?: 'left' | 'right';
}

// ── Connection Status ──
export type ConnectionStatus = 'connecting' | 'connected' | 'disconnected' | 'error';

// ── Gesture History Entry ──
export interface GestureHistoryEntry {
  gestureId: number | null;
  label: string;
  confidence: number;
  nlpText: string | null;
  timestamp: number;
  hand?: 'left' | 'right' | 'both';
}

// ── App Settings ──
export type Language = 'zh' | 'en';

export interface AppSettings {
  relayHost: string;
  show3D: boolean;
  showDashboard: boolean;
  darkMode: boolean;
  language: Language;
}
