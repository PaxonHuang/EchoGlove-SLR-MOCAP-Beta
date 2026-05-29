import { useRef, useMemo } from 'react';
import { useFrame } from '@react-three/fiber';
import { useSensorStore } from '../stores/useSensorStore';
import { eulerToQuaternion, slerpQuaternion, lerp } from '../utils/quaternion';
import {
  LERP_FACTOR,
  NUM_KEYPOINTS,
  HALL_FINGER_MAP,
} from '../utils/constants';
import type { HandPose, Keypoint3D } from '../types';

// ── Rest Pose (open hand, normalized 3D coordinates) ──
// Approximate hand model: palm ~3 units wide, fingers extend ~2.5 units
export function getRestPose(): Keypoint3D[] {
  return [
    // 0: WRIST
    { x: 0, y: 0, z: 0 },
    // 1-4: THUMB
    { x: -0.8, y: 0.5, z: -0.3 },   // CMC
    { x: -1.3, y: 1.0, z: -0.5 },   // MCP
    { x: -1.5, y: 1.6, z: -0.5 },   // IP
    { x: -1.6, y: 2.2, z: -0.5 },   // TIP
    // 5-8: INDEX
    { x: -0.5, y: 1.8, z: 0 },      // MCP
    { x: -0.5, y: 2.5, z: 0 },      // PIP
    { x: -0.5, y: 3.0, z: 0 },      // DIP
    { x: -0.5, y: 3.5, z: 0 },      // TIP
    // 9-12: MIDDLE
    { x: 0, y: 1.9, z: 0 },         // MCP
    { x: 0, y: 2.6, z: 0 },         // PIP
    { x: 0, y: 3.1, z: 0 },         // DIP
    { x: 0, y: 3.7, z: 0 },         // TIP
    // 13-16: RING
    { x: 0.5, y: 1.8, z: 0 },       // MCP
    { x: 0.5, y: 2.4, z: 0 },       // PIP
    { x: 0.5, y: 2.9, z: 0 },       // DIP
    { x: 0.5, y: 3.4, z: 0 },       // TIP
    // 17-20: PINKY
    { x: 0.9, y: 1.5, z: 0 },       // MCP
    { x: 0.9, y: 2.0, z: 0 },       // PIP
    { x: 0.9, y: 2.4, z: 0 },       // DIP
    { x: 0.9, y: 2.8, z: 0 },       // TIP
  ];
}

// ── Map hall sensor value (0-1 range) to finger curl ──
// Higher hall value = more curled
function hallToCurl(hallValue: number): number {
  // Normalize to 0..1 and clamp
  const normalized = Math.max(0, Math.min(1, (hallValue + 1) / 2));
  return normalized;
}

// ── Apply curl to finger keypoints ──
function applyFingerCurl(
  restPose: Keypoint3D[],
  hallValues: number[],
): Keypoint3D[] {
  const result = restPose.map((kp) => ({ ...kp }));

  // For each hall sensor, curl the corresponding finger joints
  for (let i = 0; i < hallValues.length && i < HALL_FINGER_MAP.length; i++) {
    const kpIndex = HALL_FINGER_MAP[i];
    const curl = hallToCurl(hallValues[i]);

    if (kpIndex > 0 && kpIndex < NUM_KEYPOINTS) {
      // Curl toward wrist: reduce Y, increase Z (fist-like motion)
      const wristY = restPose[0].y;
      const jointRestY = restPose[kpIndex].y;
      const curlOffset = (jointRestY - wristY) * curl * 0.7;

      result[kpIndex].y = jointRestY - curlOffset;
      result[kpIndex].z = curlOffset * 0.5; // slight forward push
    }
  }

  // Thumb special: curl inward (toward palm center)
  const thumbCurl = hallValues.length >= 3
    ? (hallToCurl(hallValues[1]) + hallToCurl(hallValues[2])) / 2
    : 0;
  for (const idx of [2, 3, 4]) {
    const restX = restPose[idx].x;
    result[idx].x = restX + thumbCurl * 0.6; // move thumb toward palm
    result[idx].z = restPose[idx].z - thumbCurl * 0.4;
  }

  return result;
}

// ── Hook: useHandAnimation ──
export function useHandAnimation(): HandPose {
  const restPose = useMemo(() => getRestPose(), []);

  // Mutable refs for smooth interpolation (updated every frame in useFrame)
  const currentKeypointsRef = useRef<Keypoint3D[]>(restPose);
  const currentQuaternionRef = useRef<[number, number, number, number]>([1, 0, 0, 0]);
  const targetQuaternionRef = useRef<[number, number, number, number]>([1, 0, 0, 0]);

  // Subscribe to store (read-only, updated by WebSocket)
  const hall = useSensorStore((s) => s.hall);
  const imu = useSensorStore((s) => s.imu);

  // Update targets when sensor data changes
  const targetKeypoints = useMemo(() => {
    return applyFingerCurl(restPose, hall);
  }, [restPose, hall]);

  // Update wrist quaternion from IMU (gyro for orientation)
  const imuChanged = JSON.stringify(imu);
  useMemo(() => {
    // IMU: [gyro_x, gyro_y, gyro_z, accel_x, accel_y, accel_z]
    // Use gyro as orientation estimate (simplified: integrate as euler angles)
    const gyroX = imu.length > 0 ? imu[0] : 0;
    const gyroY = imu.length > 1 ? imu[1] : 0;
    const gyroZ = imu.length > 2 ? imu[2] : 0;

    // Convert gyro readings (rad/s conceptual) to small euler angles
    const roll = (gyroX / 500) * Math.PI;     // Scale down significantly
    const pitch = (gyroY / 500) * Math.PI;
    const yaw = (gyroZ / 500) * Math.PI;

    targetQuaternionRef.current = eulerToQuaternion(roll, pitch, yaw);
  }, [imuChanged]); // eslint-disable-line react-hooks/exhaustive-deps

  // Smooth interpolation every frame
  useFrame(() => {
    // Lerp keypoints
    for (let i = 0; i < NUM_KEYPOINTS; i++) {
      const current = currentKeypointsRef.current[i];
      const target = targetKeypoints[i];
      currentKeypointsRef.current[i] = {
        x: lerp(current.x, target.x, LERP_FACTOR),
        y: lerp(current.y, target.y, LERP_FACTOR),
        z: lerp(current.z, target.z, LERP_FACTOR),
      };
    }

    // SLERP quaternion
    currentQuaternionRef.current = slerpQuaternion(
      currentQuaternionRef.current,
      targetQuaternionRef.current,
      LERP_FACTOR,
    );
  });

  return {
    get keypoints() {
      return currentKeypointsRef.current;
    },
    get quaternion() {
      return currentQuaternionRef.current;
    },
  } as HandPose;
}
