import { useRef, useMemo } from 'react';
import { useFrame } from '@react-three/fiber';
import { useSensorStore } from '../stores/useSensorStore';
import { eulerToQuaternion, slerpQuaternion, lerp } from '../utils/quaternion';
import {
  LERP_FACTOR,
  NUM_KEYPOINTS,
  FLEX_FINGER_MAP,
} from '../utils/constants';
import type { HandPose, Keypoint3D } from '../types';

// ── Rest Pose (open hand, normalized 3D coordinates) ──
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

// ── Apply V5 flex sensor curl to finger keypoints ──
// Each flex sensor [0,1] controls one finger's MCP/PIP/DIP via 2:1 cascade
function applyFlexCurl(
  restPose: Keypoint3D[],
  flexValues: number[],
): Keypoint3D[] {
  const result = restPose.map((kp) => ({ ...kp }));

  for (const mapping of FLEX_FINGER_MAP) {
    const curl = Math.max(0, Math.min(1, flexValues[mapping.sensorIdx] || 0));

    // MCP curl (full amount)
    const mcpRest = restPose[mapping.mcpKeypoint];
    const wristY = restPose[0].y;
    const mcpCurlOffset = (mcpRest.y - wristY) * curl * 0.7;
    result[mapping.mcpKeypoint].y = mcpRest.y - mcpCurlOffset;
    result[mapping.mcpKeypoint].z = mcpRest.z + mcpCurlOffset * 0.5;

    // PIP curl (2/3 of MCP)
    const pipRest = restPose[mapping.pipKeypoint];
    const pipCurlOffset = (pipRest.y - wristY) * curl * 0.7 * 0.67;
    result[mapping.pipKeypoint].y = pipRest.y - pipCurlOffset;
    result[mapping.pipKeypoint].z = pipRest.z + pipCurlOffset * 0.5;

    // DIP curl (1/3 of MCP)
    const dipRest = restPose[mapping.dipKeypoint];
    const dipCurlOffset = (dipRest.y - wristY) * curl * 0.7 * 0.33;
    result[mapping.dipKeypoint].y = dipRest.y - dipCurlOffset;
    result[mapping.dipKeypoint].z = dipRest.z + dipCurlOffset * 0.5;
  }

  // Thumb special: curl inward (toward palm center)
  const thumbCurl = flexValues.length > 0 ? Math.max(0, Math.min(1, flexValues[0])) : 0;
  for (const idx of [2, 3, 4]) {
    const restX = restPose[idx].x;
    result[idx].x = restX + thumbCurl * 0.6;
    result[idx].z = restPose[idx].z - thumbCurl * 0.4;
  }

  return result;
}

// ── V3 backward compat: map hall[15] to flex[5] ──
function hallToFlex(hallValues: number[]): number[] {
  if (hallValues.length < 5) return [0, 0, 0, 0, 0];
  // V3 hall: 15 values, average groups of 3 per finger
  return [
    (hallValues[0] + hallValues[1] + hallValues[2]) / 3,  // thumb
    (hallValues[3] + hallValues[4] + hallValues[5]) / 3,  // index
    (hallValues[6] + hallValues[7] + hallValues[8]) / 3,  // middle
    (hallValues[9] + hallValues[10] + hallValues[11]) / 3, // ring
    (hallValues[12] + hallValues[13] + hallValues[14]) / 3, // pinky
  ];
}

interface UseHandAnimationOptions {
  handedness?: 'left' | 'right';
}

// ── Hook: useHandAnimation ──
export function useHandAnimation(options: UseHandAnimationOptions = {}): HandPose {
  const { handedness = 'left' } = options;
  const restPose = useMemo(() => getRestPose(), []);

  const currentKeypointsRef = useRef<Keypoint3D[]>(restPose);
  const currentQuaternionRef = useRef<[number, number, number, number]>([1, 0, 0, 0]);
  const targetQuaternionRef = useRef<[number, number, number, number]>([1, 0, 0, 0]);

  // Read per-hand data from store
  const handData = useSensorStore((s) =>
    handedness === 'left' ? s.leftHand : s.rightHand
  );
  const hall = useSensorStore((s) => s.hall);
  const imu = useSensorStore((s) => s.imu);

  // Compute flex values: prefer V5 per-hand, fallback to V3 hall
  const flexValues = useMemo(() => {
    if (handData.flex.some((v: number) => v !== 0)) {
      return handData.flex;
    }
    // V3 fallback
    return hallToFlex(hall);
  }, [handData.flex, hall]);

  // Target keypoints from flex curl
  const targetKeypoints = useMemo(() => {
    return applyFlexCurl(restPose, flexValues);
  }, [restPose, flexValues]);

  // Wrist quaternion from euler (V5) or gyro (V3)
  const eulerStr = JSON.stringify(handData.euler);
  useMemo(() => {
    if (handData.euler.some((v: number) => v !== 0)) {
      // V5: euler angles in degrees → radians
      const roll = (handData.euler[0] || 0) * Math.PI / 180;
      const pitch = (handData.euler[1] || 0) * Math.PI / 180;
      const yaw = (handData.euler[2] || 0) * Math.PI / 180;
      targetQuaternionRef.current = eulerToQuaternion(roll, pitch, yaw);
    } else {
      // V3 fallback: gyro → euler approximation
      const gyroX = imu.length > 0 ? imu[0] : 0;
      const gyroY = imu.length > 1 ? imu[1] : 0;
      const gyroZ = imu.length > 2 ? imu[2] : 0;
      const roll = (gyroX / 500) * Math.PI;
      const pitch = (gyroY / 500) * Math.PI;
      const yaw = (gyroZ / 500) * Math.PI;
      targetQuaternionRef.current = eulerToQuaternion(roll, pitch, yaw);
    }
  }, [eulerStr]); // eslint-disable-line react-hooks/exhaustive-deps

  // Smooth interpolation every frame
  useFrame(() => {
    for (let i = 0; i < NUM_KEYPOINTS; i++) {
      const current = currentKeypointsRef.current[i];
      const target = targetKeypoints[i];
      currentKeypointsRef.current[i] = {
        x: lerp(current.x, target.x, LERP_FACTOR),
        y: lerp(current.y, target.y, LERP_FACTOR),
        z: lerp(current.z, target.z, LERP_FACTOR),
      };
    }
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
    handedness,
  } as HandPose;
}
