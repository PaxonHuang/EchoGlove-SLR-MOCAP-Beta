// ── useFlexToAngles: 校准 flex[5] → 15 关节角（度）FK 驱动 ──
// flex 归一化 0=straight..1=bent（demo_server 已用校准范围重映射）。
// 解剖学比例：真拳头中 PIP 屈于 MCP 多，DIP 最少；拇指对掌近似为 CMC 旋转。
// 输出顺序同 handKinematics JointAngles：
//   [thumb_CMC, thumb_MCP, thumb_IP,
//    index_MCP, index_PIP, index_DIP,
//    middle_MCP, middle_PIP, middle_DIP,
//    ring_MCP, ring_PIP, ring_DIP,
//    pinky_MCP, pinky_PIP, pinky_DIP]
// ch3/ring 硬件故障 → ring 近静止（已知限制）。
// lerp 平滑（0.2）杀抖动。

import { useRef } from 'react';
import { useFrame } from '@react-three/fiber';
import { useSensorStore } from '../stores/useSensorStore';
import { lerpAngles } from '../utils/handKinematics';

const REST_ANGLES: number[] = new Array(15).fill(0);

// flex → 该指 [mcp, pip, dip] 度（线性）
function fingerFlexToAngles(flex: number, mcpMax: number, pipMax: number, dipMax: number): [number, number, number] {
  const f = Math.max(0, Math.min(1, flex));
  return [f * mcpMax, f * pipMax, f * dipMax];
}

// 比例（度）：MCP<PIP，DIP 最少；拇指用 CMC/MCP/IP
const FINGER_RATIOS = { mcp: 85, pip: 105, dip: 70 };
const THUMB_RATIOS = { cmc: 35, mcp: 55, ip: 75 };

export function flexToJointAngles(flex: number[]): number[] {
  const f = (i: number) => Math.max(0, Math.min(1, flex[i] ?? 0));
  // 拇指：CMC=flex×35, MCP=flex×55, IP=flex×75
  const thumb = [f(0) * THUMB_RATIOS.cmc, f(0) * THUMB_RATIOS.mcp, f(0) * THUMB_RATIOS.ip];

  const index = fingerFlexToAngles(f(1), FINGER_RATIOS.mcp, FINGER_RATIOS.pip, FINGER_RATIOS.dip);
  const middle = fingerFlexToAngles(f(2), FINGER_RATIOS.mcp, FINGER_RATIOS.pip, FINGER_RATIOS.dip);
  const ring = fingerFlexToAngles(f(3), FINGER_RATIOS.mcp, FINGER_RATIOS.pip, FINGER_RATIOS.dip);
  const pinky = fingerFlexToAngles(f(4), FINGER_RATIOS.mcp, FINGER_RATIOS.pip, FINGER_RATIOS.dip);

  return [
    thumb[0], thumb[1], thumb[2],
    index[0], index[1], index[2],
    middle[0], middle[1], middle[2],
    ring[0], ring[1], ring[2],
    pinky[0], pinky[1], pinky[2],
  ];
}

// ── Hook: 每帧读 leftHand.flex，平滑后输出 15 关节角 ──
// 不渲染任何 DOM，只返回 ref 当前值供渲染组件 useFrame 取用。
export function useFlexToAngles() {
  const currentRef = useRef<number[]>(REST_ANGLES);
  const targetRef = useRef<number[]>(REST_ANGLES);

  const leftFlex = useSensorStore((s) => s.leftHand.flex);

  useFrame(() => {
    targetRef.current = flexToJointAngles(leftFlex);
    currentRef.current = lerpAngles(currentRef.current, targetRef.current, 0.2);
  });

  return { currentRef, targetRef };
}
