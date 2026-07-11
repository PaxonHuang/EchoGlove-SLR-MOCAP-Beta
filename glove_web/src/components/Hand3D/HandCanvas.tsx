import { Canvas } from '@react-three/fiber';
import { OrbitControls, Grid, PerspectiveCamera } from '@react-three/drei';
import { LiveHandRenderer } from './LiveHandRenderer';
import { COLORS } from '../../utils/constants';

// ── HandCanvas: 仪表盘画布 ──
// 用户选择：只显左手居中（FK 关节链实时 MOCAP）。
// 移除右手 + LEFT/RIGHT 标签；相机聚焦单手。
export function HandCanvas() {
  return (
    <div className="h-full w-full">
      <Canvas
        gl={{ antialias: true, alpha: false }}
        dpr={[1, 2]}
        style={{ background: COLORS.background }}
      >
        {/* Camera — 单手居中，拉近 */}
        <PerspectiveCamera makeDefault position={[0, 1.5, 5]} fov={50} />

        {/* Lighting */}
        <ambientLight intensity={0.6} />
        <directionalLight position={[5, 8, 5]} intensity={0.8} castShadow />
        <directionalLight position={[-3, 4, -3]} intensity={0.3} />

        {/* Ground Grid */}
        <Grid
          args={[10, 10]}
          cellSize={0.5}
          cellThickness={0.5}
          cellColor="#1e293b"
          sectionSize={2}
          sectionThickness={1}
          sectionColor="#334155"
          fadeDistance={15}
          position={[0, -0.5, 0]}
          rotation={[0, 0, 0]}
          infiniteGrid
        />

        {/* 左手 FK 实时 MOCAP（居中）*/}
        <LiveHandRenderer position={[0, 0.3, 0]} />

        {/* Orbit Controls */}
        <OrbitControls
          enablePan={false}
          enableZoom={true}
          minDistance={2}
          maxDistance={10}
          target={[0, 0.8, 0]}
        />
      </Canvas>
    </div>
  );
}

export default HandCanvas;
