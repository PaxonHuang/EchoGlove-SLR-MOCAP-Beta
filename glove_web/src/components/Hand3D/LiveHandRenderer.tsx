// ── LiveHandRenderer: 仪表盘实时 MOCAP FK 手 ──
// 每帧读 useSensorStore.leftHand.flex（校准 0..1）→ 15 关节角 → FingerChain。
// 左手居中、放大；IMU=zeros → 腕固定略前倾便于观察手型。
// ch3/ring 硬件故障 → 无名指近静止（已知）。

import { useRef } from 'react';
import { useFrame } from '@react-three/fiber';
import * as THREE from 'three';
import { FullHandChain, useFingerColors } from './FingerChain';
import { useFlexToAngles } from '../../hooks/useFlexToAngles';

// 腕固定姿态：略前倾（绕 X 负向小幅旋转，掌心朝向相机）
const FIXED_WRIST_QUAT = new THREE.Quaternion().setFromAxisAngle(
  new THREE.Vector3(1, 0, 0),
  THREE.MathUtils.degToRad(-15),
);

export function LiveHandRenderer({ position = [0, 0.5, 0] as [number, number, number] }) {
  const { currentRef } = useFlexToAngles();
  const colorMap = useFingerColors();
  const groupRef = useRef<THREE.Group>(null);

  useFrame(() => {
    if (groupRef.current) {
      groupRef.current.quaternion.copy(FIXED_WRIST_QUAT);
    }
  });

  // 放大×2.2 便于观察关节动画
  return (
    <group ref={groupRef} position={position} scale={[2.2, 2.2, 2.2]}>
      <FullHandChain jointAngles={currentRef.current} colorMap={colorMap} />
    </group>
  );
}
