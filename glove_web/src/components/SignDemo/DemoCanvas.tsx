// ── DemoCanvas: 手语教学页 R3F Canvas ──
// 全画面清晰显示手部，居中放大；相机框定整手（含手掌/前臂）。
// 不溢出边界：相机距离/FOV 经校准，min/max 距离限制缩放范围。

import { Canvas } from '@react-three/fiber';
import { OrbitControls, Grid, Environment } from '@react-three/drei';
import { DemoHandRenderer } from '../Hand3D/DemoHandRenderer';

export function DemoCanvas() {
  return (
    <Canvas
      camera={{ position: [0, 0.1, 1.6], fov: 45 }}
      gl={{ antialias: true, alpha: false }}
      style={{ background: '#0a0f1e' }}
    >
      {/* 相机控制：聚焦手部中心，限定缩放范围防脱框 */}
      <OrbitControls
        enablePan={false}
        minDistance={1.2}
        maxDistance={3.0}
        target={[0, 0.25, 0]}
        maxPolarAngle={Math.PI * 0.82}
        minPolarAngle={Math.PI * 0.18}
      />

      {/* 光照 */}
      <ambientLight intensity={0.55} />
      <directionalLight position={[5, 8, 5]} intensity={1.1} castShadow />
      <directionalLight position={[-3, 4, -2]} intensity={0.45} color="#64748b" />

      {/* 环境反射 */}
      <Environment preset="night" />

      {/* 地面网格（远景淡出，衬托手部）*/}
      <Grid
        position={[0, -0.18, 0]}
        args={[20, 20]}
        cellSize={0.5}
        cellThickness={0.5}
        cellColor="#1e293b"
        sectionSize={2}
        sectionThickness={1}
        sectionColor="#334155"
        fadeDistance={10}
        fadeStrength={1.5}
        infiniteGrid
      />

      {/* FK 手（含手掌+前臂），居中 */}
      <DemoHandRenderer />
    </Canvas>
  );
}

export default DemoCanvas;
