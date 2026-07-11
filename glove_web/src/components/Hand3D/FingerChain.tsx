// ── FingerChain: 共享 FK 手指渲染 ──
// 嵌套旋转组 MCP→PIP→DIP 绕局部 X 轴（屈曲）。
// 仪表盘 LiveHandRenderer 与教学页 DemoHandRenderer 共用，避免 FK 数学重复。
// jointAngles 为 15 维度数组（顺序见 handKinematics JointAngles 注释）。

import { useMemo } from 'react';
import { degToRad } from '../../utils/handKinematics';
import { FINGER_GROUPS, COLORS } from '../../utils/constants';

// ── 指段长度（米）──
export const SEGMENTS = {
  thumb: { seg1: 0.035, seg2: 0.03, seg3: 0.02 },
  index: { seg1: 0.04, seg2: 0.025, seg3: 0.02 },
  middle: { seg1: 0.045, seg2: 0.028, seg3: 0.02 },
  ring: { seg1: 0.04, seg2: 0.025, seg3: 0.018 },
  pinky: { seg1: 0.035, seg2: 0.02, seg3: 0.015 },
};

// ── 静止位 MCP 位置（相对手腕原点）──
export const REST_MCP = {
  thumb: [-0.04, 0.03, 0.01] as [number, number, number],
  index: [-0.03, 0.0, 0] as [number, number, number],
  middle: [-0.01, 0.005, 0] as [number, number, number],
  ring: [0.015, 0.0, 0] as [number, number, number],
  pinky: [0.035, -0.01, 0] as [number, number, number],
};

export type FingerName = keyof typeof SEGMENTS;

// ── 骨干网格 ──
export function Bone({ length, color }: { length: number; color: string }) {
  const midY = length / 2;
  return (
    <mesh position={[0, midY, 0]}>
      <cylinderGeometry args={[0.006, 0.005, length, 8]} />
      <meshStandardMaterial color={color} roughness={0.4} metalness={0.1} />
    </mesh>
  );
}

// ── 关节球 ──
export function JointSphere({ color, radius = 0.008 }: { color: string; radius?: number }) {
  return (
    <mesh>
      <sphereGeometry args={[radius, 10, 10]} />
      <meshStandardMaterial
        color={color}
        roughness={0.35}
        metalness={0.15}
        emissive={color}
        emissiveIntensity={0.15}
      />
    </mesh>
  );
}

// ── 单指 FK 链 ──
export function FingerChain({
  name,
  jointAngles,
  color,
}: {
  name: FingerName;
  jointAngles: number[]; // [mcp, pip, dip] 度
  color: string;
}) {
  const seg = SEGMENTS[name];
  const mcpRad = degToRad(jointAngles[0]);
  const pipRad = degToRad(jointAngles[1]);
  const dipRad = degToRad(jointAngles[2]);

  return (
    <group position={REST_MCP[name]}>
      {/* MCP */}
      <group rotation={[mcpRad, 0, 0]}>
        <JointSphere color={color} radius={0.009} />
        <Bone length={seg.seg1} color={color} />

        {/* PIP */}
        <group position={[0, seg.seg1, 0]} rotation={[pipRad, 0, 0]}>
          <JointSphere color={color} radius={0.007} />
          <Bone length={seg.seg2} color={color} />

          {/* DIP */}
          <group position={[0, seg.seg2, 0]} rotation={[dipRad, 0, 0]}>
            <JointSphere color={color} radius={0.006} />
            <Bone length={seg.seg3} color={color} />

            {/* 指尖 */}
            <group position={[0, seg.seg3, 0]}>
              <JointSphere color={color} radius={0.005} />
            </group>
          </group>
        </group>
      </group>
    </group>
  );
}

// ── 手指名→颜色 映射（FINGER_GROUPS）──
export function useFingerColors(): Record<string, string> {
  return useMemo(() => {
    const map: Record<string, string> = {};
    for (const group of FINGER_GROUPS) {
      map[group.name.toLowerCase()] = group.color;
    }
    return map;
  }, []);
}

// ── 整手 FK 链（5 指，15 关节角）──
// 由调用方提供 jointAngles[15]；wristQuat 由外层 group 应用。
export function FullHandChain({
  jointAngles,
  colorMap,
}: {
  jointAngles: number[]; // 15 维
  colorMap: Record<string, string>;
}) {
  return (
    <>
      {/* 腕球 */}
      <JointSphere color={COLORS.wrist} radius={0.012} />

      {/* 拇指 [0,1,2] */}
      <FingerChain
        name="thumb"
        jointAngles={[jointAngles[0], jointAngles[1], jointAngles[2]]}
        color={colorMap.thumb || '#ef4444'}
      />
      {/* 食指 [3,4,5] */}
      <FingerChain
        name="index"
        jointAngles={[jointAngles[3], jointAngles[4], jointAngles[5]]}
        color={colorMap.index || '#f59e0b'}
      />
      {/* 中指 [6,7,8] */}
      <FingerChain
        name="middle"
        jointAngles={[jointAngles[6], jointAngles[7], jointAngles[8]]}
        color={colorMap.middle || '#22c55e'}
      />
      {/* 无名指 [9,10,11] */}
      <FingerChain
        name="ring"
        jointAngles={[jointAngles[9], jointAngles[10], jointAngles[11]]}
        color={colorMap.ring || '#3b82f6'}
      />
      {/* 小指 [12,13,14] */}
      <FingerChain
        name="pinky"
        jointAngles={[jointAngles[12], jointAngles[13], jointAngles[14]]}
        color={colorMap.pinky || '#a855f7'}
      />
    </>
  );
}
