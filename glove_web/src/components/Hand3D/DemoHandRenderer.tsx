// ── DemoHandRenderer: FK-based 3D hand skeleton ──
// Uses nested rotation groups for anatomically correct finger animation.
// Each finger: MCP group (rotationX) → PIP group (rotationX) → DIP group (rotationX)
// Bone meshes connect each joint level. Wrist quaternion applied to parent group.

import { useRef, useMemo } from 'react';
import { useFrame } from '@react-three/fiber';
import * as THREE from 'three';
import { useDemoAnimation } from '../../hooks/useDemoAnimation';
import { degToRad } from '../../utils/handKinematics';
import { FINGER_GROUPS, COLORS } from '../../utils/constants';

// ── Segment lengths ──
const SEGMENTS = {
  thumb: { seg1: 0.035, seg2: 0.03, seg3: 0.02 },
  index: { seg1: 0.04, seg2: 0.025, seg3: 0.02 },
  middle: { seg1: 0.045, seg2: 0.028, seg3: 0.02 },
  ring: { seg1: 0.04, seg2: 0.025, seg3: 0.018 },
  pinky: { seg1: 0.035, seg2: 0.02, seg3: 0.015 },
};

// ── Rest pose MCP positions (relative to wrist origin) ──
const REST_MCP = {
  thumb: [-0.04, 0.03, 0.01] as [number, number, number],
  index: [-0.03, 0.0, 0] as [number, number, number],
  middle: [-0.01, 0.005, 0] as [number, number, number],
  ring: [0.015, 0.0, 0] as [number, number, number],
  pinky: [0.035, -0.01, 0] as [number, number, number],
};

// ── Bone mesh component ──
function Bone({ length, color }: { length: number; color: string }) {
  const midY = length / 2;
  return (
    <mesh position={[0, midY, 0]}>
      <cylinderGeometry args={[0.006, 0.005, length, 8]} />
      <meshStandardMaterial color={color} roughness={0.4} metalness={0.1} />
    </mesh>
  );
}

// ── Joint sphere ──
function JointSphere({ color, radius = 0.008 }: { color: string; radius?: number }) {
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

// ── Finger chain with nested rotation groups ──
function FingerChain({
  name,
  jointAngles,
  color,
}: {
  name: keyof typeof SEGMENTS;
  jointAngles: number[]; // [mcp, pip, dip] in degrees
  color: string;
}) {
  const seg = SEGMENTS[name];
  const mcpRad = degToRad(jointAngles[0]);
  const pipRad = degToRad(jointAngles[1]);
  const dipRad = degToRad(jointAngles[2]);

  return (
    <group position={REST_MCP[name]}>
      {/* MCP joint */}
      <group rotation={[mcpRad, 0, 0]}>
        <JointSphere color={color} radius={0.009} />
        <Bone length={seg.seg1} color={color} />

        {/* PIP joint */}
        <group position={[0, seg.seg1, 0]} rotation={[pipRad, 0, 0]}>
          <JointSphere color={color} radius={0.007} />
          <Bone length={seg.seg2} color={color} />

          {/* DIP joint */}
          <group position={[0, seg.seg2, 0]} rotation={[dipRad, 0, 0]}>
            <JointSphere color={color} radius={0.006} />
            <Bone length={seg.seg3} color={color} />

            {/* Fingertip */}
            <group position={[0, seg.seg3, 0]}>
              <JointSphere color={color} radius={0.005} />
            </group>
          </group>
        </group>
      </group>
    </group>
  );
}

// ── Main renderer ──
export function DemoHandRenderer() {
  const { jointAngles, wristQuat, wristPos } = useDemoAnimation();
  const groupRef = useRef<THREE.Group>(null);

  // Apply wrist quaternion smoothly (slerp to avoid jitter)
  useFrame(() => {
    if (groupRef.current) {
      const [w, x, y, z] = wristQuat;
      const targetQuat = new THREE.Quaternion(x, y, z, w);
      groupRef.current.quaternion.slerp(targetQuat, 0.12);
      groupRef.current.position.set(wristPos[0], wristPos[1] + 0.5, wristPos[2]);
    }
  });

  // Get finger colors from FINGER_GROUPS
  const colorMap = useMemo(() => {
    const map: Record<string, string> = {};
    for (const group of FINGER_GROUPS) {
      map[group.name.toLowerCase()] = group.color;
    }
    return map;
  }, []);

  return (
    <group ref={groupRef} position={[0, 0.5, 0]}>
      {/* Wrist joint sphere */}
      <JointSphere color={COLORS.wrist} radius={0.012} />

      {/* Thumb (CMC at rest_mcp, uses joint indices 0,1,2) */}
      <FingerChain
        name="thumb"
        jointAngles={[jointAngles[0], jointAngles[1], jointAngles[2]]}
        color={colorMap.thumb || '#ef4444'}
      />

      {/* Index (uses joint indices 3,4,5) */}
      <FingerChain
        name="index"
        jointAngles={[jointAngles[3], jointAngles[4], jointAngles[5]]}
        color={colorMap.index || '#f59e0b'}
      />

      {/* Middle (uses joint indices 6,7,8) */}
      <FingerChain
        name="middle"
        jointAngles={[jointAngles[6], jointAngles[7], jointAngles[8]]}
        color={colorMap.middle || '#22c55e'}
      />

      {/* Ring (uses joint indices 9,10,11) */}
      <FingerChain
        name="ring"
        jointAngles={[jointAngles[9], jointAngles[10], jointAngles[11]]}
        color={colorMap.ring || '#3b82f6'}
      />

      {/* Pinky (uses joint indices 12,13,14) */}
      <FingerChain
        name="pinky"
        jointAngles={[jointAngles[12], jointAngles[13], jointAngles[14]]}
        color={colorMap.pinky || '#a855f7'}
      />
    </group>
  );
}
