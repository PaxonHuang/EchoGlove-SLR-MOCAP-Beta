// ── DemoCanvas: R3F Canvas for the sign language demo ──
// Optimized for fullscreen layout — closer camera, better framing.

import { Canvas } from '@react-three/fiber';
import { OrbitControls, Grid, Environment } from '@react-three/drei';
import { DemoHandRenderer } from '../Hand3D/DemoHandRenderer';

export function DemoCanvas() {
  return (
    <Canvas
      camera={{ position: [0, 0.9, 2.0], fov: 50 }}
      gl={{ antialias: true, alpha: false }}
      style={{ background: '#0a0f1e' }}
    >
      {/* Camera controls — closer view, hand-centered */}
      <OrbitControls
        enablePan={false}
        minDistance={1.0}
        maxDistance={5.0}
        target={[0, 0.5, 0]}
        maxPolarAngle={Math.PI * 0.85}
        minPolarAngle={Math.PI * 0.1}
      />

      {/* Lighting */}
      <ambientLight intensity={0.5} />
      <directionalLight position={[5, 8, 5]} intensity={1.0} castShadow />
      <directionalLight position={[-3, 4, -2]} intensity={0.4} color="#64748b" />

      {/* Environment */}
      <Environment preset="night" />

      {/* Ground grid */}
      <Grid
        position={[0, -0.01, 0]}
        args={[20, 20]}
        cellSize={0.5}
        cellThickness={0.5}
        cellColor="#1e293b"
        sectionSize={2}
        sectionThickness={1}
        sectionColor="#334155"
        fadeDistance={12}
        fadeStrength={1.5}
        infiniteGrid
      />

      {/* Hand skeleton */}
      <DemoHandRenderer />
    </Canvas>
  );
}