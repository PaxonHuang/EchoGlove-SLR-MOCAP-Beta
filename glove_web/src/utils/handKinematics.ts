// ── Hand Kinematics: Forward Kinematics for anatomically correct hand animation ──
// Uses rotation group hierarchy: each joint rotates around local X-axis (flexion).
// Rest pose: fingers pointing +Y, palm facing +Z (hand back -Z), thumb +X.

import * as THREE from 'three';

// ── Segment lengths (meters) ──
export const SEGMENT_LENGTHS = {
  thumb: { cmcToMcp: 0.035, mcpToIp: 0.03, ipToTip: 0.02 },
  index: { mcpToPip: 0.04, pipToDip: 0.025, dipToTip: 0.02 },
  middle: { mcpToPip: 0.045, pipToDip: 0.028, dipToTip: 0.02 },
  ring: { mcpToPip: 0.04, pipToDip: 0.025, dipToTip: 0.018 },
  pinky: { mcpToPip: 0.035, pipToDip: 0.02, dipToTip: 0.015 },
};

// ── Rest pose MCP positions (relative to wrist) ──
export const REST_MCP = {
  thumb: new THREE.Vector3(-0.04, 0.03, 0.01),
  index: new THREE.Vector3(-0.03, 0.0, 0),
  middle: new THREE.Vector3(-0.01, 0.005, 0),
  ring: new THREE.Vector3(0.015, 0.0, 0),
  pinky: new THREE.Vector3(0.035, -0.01, 0),
};

// ── Joint angle structure ──
// 15 angles in degrees: [thumb_CMC, thumb_MCP, thumb_IP, index_MCP, index_PIP, index_DIP,
//                         middle_MCP, middle_PIP, middle_DIP, ring_MCP, ring_PIP, ring_DIP,
//                         pinky_MCP, pinky_PIP, pinky_DIP]
export interface JointAngles {
  angles: number[]; // 15 joint angles in degrees
}

// ── Convert degrees to radians ──
export function degToRad(deg: number): number {
  return (deg * Math.PI) / 180;
}

// ── Compute world position of a point in a finger chain ──
// Given MCP position, joint angles (in degrees), and segment lengths,
// compute the world position of the fingertip.
export function computeFingerTipWorld(
  mcpPos: THREE.Vector3,
  angles: number[], // [mcp, pip, dip] in degrees
  seg1: number, // mcpToPip
  seg2: number, // pipToDip
  seg3: number, // dipToTip
  wristQuat: THREE.Quaternion,
): THREE.Vector3 {
  const mcpRad = degToRad(angles[0]);
  const pipRad = degToRad(angles[1]);
  const dipRad = degToRad(angles[2]);

  // Build cumulative rotation at each joint
  const mcpQuat = new THREE.Quaternion().setFromAxisAngle(
    new THREE.Vector3(1, 0, 0), mcpRad
  );
  const pipQuat = new THREE.Quaternion().setFromAxisAngle(
    new THREE.Vector3(1, 0, 0), pipRad
  );
  const dipQuat = new THREE.Quaternion().setFromAxisAngle(
    new THREE.Vector3(1, 0, 0), dipRad
  );

  // Cumulative rotation at MCP = wrist * mcp
  const cumMcp = new THREE.Quaternion().multiplyQuaternions(wristQuat, mcpQuat);
  // At PIP = cumMcp * pip
  const cumPip = new THREE.Quaternion().multiplyQuaternions(cumMcp, pipQuat);
  // At DIP = cumPip * dip
  const cumDip = new THREE.Quaternion().multiplyQuaternions(cumPip, dipQuat);

  // Compute world positions
  const upDir = new THREE.Vector3(0, 1, 0);

  // PIP position = MCP + cumMcp * (0, seg1, 0)
  const pipOffset = upDir.clone().multiplyScalar(seg1).applyQuaternion(cumMcp);
  const pipPos = mcpPos.clone().add(pipOffset);

  // DIP position = PIP + cumPip * (0, seg2, 0)
  const dipOffset = upDir.clone().multiplyScalar(seg2).applyQuaternion(cumPip);
  const dipPos = pipPos.clone().add(dipOffset);

  // Tip position = DIP + cumDip * (0, seg3, 0)
  const tipOffset = upDir.clone().multiplyScalar(seg3).applyQuaternion(cumDip);
  const tipPos = dipPos.clone().add(tipOffset);

  return tipPos;
}

// ── Compute all joint world positions for a finger ──
// Returns: [mcp, pip, dip, tip] world positions
export function computeFingerChainWorld(
  mcpPos: THREE.Vector3,
  angles: number[], // [mcp, pip, dip] in degrees
  seg1: number,
  seg2: number,
  seg3: number,
  wristQuat: THREE.Quaternion,
): [THREE.Vector3, THREE.Vector3, THREE.Vector3, THREE.Vector3] {
  const mcpRad = degToRad(angles[0]);
  const pipRad = degToRad(angles[1]);
  const dipRad = degToRad(angles[2]);

  const mcpQuat = new THREE.Quaternion().setFromAxisAngle(
    new THREE.Vector3(1, 0, 0), mcpRad
  );
  const pipQuat = new THREE.Quaternion().setFromAxisAngle(
    new THREE.Vector3(1, 0, 0), pipRad
  );
  const dipQuat = new THREE.Quaternion().setFromAxisAngle(
    new THREE.Vector3(1, 0, 0), dipRad
  );

  const cumMcp = new THREE.Quaternion().multiplyQuaternions(wristQuat, mcpQuat);
  const cumPip = new THREE.Quaternion().multiplyQuaternions(cumMcp, pipQuat);
  const cumDip = new THREE.Quaternion().multiplyQuaternions(cumPip, dipQuat);

  const upDir = new THREE.Vector3(0, 1, 0);

  const pipOffset = upDir.clone().multiplyScalar(seg1).applyQuaternion(cumMcp);
  const pipPos = mcpPos.clone().add(pipOffset);

  const dipOffset = upDir.clone().multiplyScalar(seg2).applyQuaternion(cumPip);
  const dipPos = pipPos.clone().add(dipOffset);

  const tipOffset = upDir.clone().multiplyScalar(seg3).applyQuaternion(cumDip);
  const tipPos = dipPos.clone().add(tipOffset);

  return [mcpPos, pipPos, dipPos, tipPos];
}

// ── Interpolate joint angles ──
export function lerpAngles(from: number[], to: number[], t: number): number[] {
  return from.map((f, i) => f + (to[i] - f) * t);
}

// ── Easing functions ──
export function easeInOutQuad(t: number): number {
  return t < 0.5 ? 2 * t * t : 1 - Math.pow(-2 * t + 2, 2) / 2;
}

export function easeOutCubic(t: number): number {
  return 1 - Math.pow(1 - t, 3);
}
