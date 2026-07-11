// ── DemoHandRenderer: 教学页 FK 3D 手（改用共享 FingerChain）──
// 行为不变：useDemoAnimation 提供 jointAngles/wristQuat/wristPos，
// 仅把内联 FK 链替换为共享 FullHandChain，避免 FK 数学重复。
// 放大×2.4 让手部占据画布主体；腕基座上移居中。

import { useRef } from 'react';
import { useFrame } from '@react-three/fiber';
import * as THREE from 'three';
import { useDemoAnimation } from '../../hooks/useDemoAnimation';
import { FullHandChain, useFingerColors } from './FingerChain';

export function DemoHandRenderer() {
  const { jointAngles, wristQuat, wristPos } = useDemoAnimation();
  const groupRef = useRef<THREE.Group>(null);
  const colorMap = useFingerColors();

  // 腕四元数平滑（slerp 防抖）+ 基座居中
  useFrame(() => {
    if (groupRef.current) {
      const [w, x, y, z] = wristQuat;
      const targetQuat = new THREE.Quaternion(x, y, z, w);
      groupRef.current.quaternion.slerp(targetQuat, 0.12);
      groupRef.current.position.set(wristPos[0], wristPos[1] + 0.35, wristPos[2]);
    }
  });

  return (
    <group ref={groupRef} position={[0, 0.35, 0]} scale={[2.4, 2.4, 2.4]}>
      <FullHandChain jointAngles={jointAngles} colorMap={colorMap} />
    </group>
  );
}
