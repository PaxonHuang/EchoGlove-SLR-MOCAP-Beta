import { Canvas } from '@react-three/fiber';
import { OrbitControls, Grid, PerspectiveCamera, Text } from '@react-three/drei';
import HandSkeleton from './HandSkeleton';
import { COLORS } from '../../utils/constants';

export function HandCanvas() {
  return (
    <div className="h-full w-full">
      <Canvas
        gl={{ antialias: true, alpha: false }}
        dpr={[1, 2]}
        style={{ background: COLORS.background }}
      >
        {/* Camera — pulled back to see both hands */}
        <PerspectiveCamera makeDefault position={[0, 3, 8]} fov={50} />

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

        {/* Left Hand */}
        <HandSkeleton handedness="left" position={[-1.8, 0.5, 0]} />
        <Text
          position={[-1.8, -0.2, 0]}
          fontSize={0.2}
          color="#94a3b8"
          anchorX="center"
          anchorY="top"
        >
          LEFT
        </Text>

        {/* Right Hand */}
        <HandSkeleton handedness="right" position={[1.8, 0.5, 0]} />
        <Text
          position={[1.8, -0.2, 0]}
          fontSize={0.2}
          color="#94a3b8"
          anchorX="center"
          anchorY="top"
        >
          RIGHT
        </Text>

        {/* Orbit Controls */}
        <OrbitControls
          enablePan={false}
          enableZoom={true}
          minDistance={3}
          maxDistance={15}
          target={[0, 1.5, 0]}
        />
      </Canvas>
    </div>
  );
}

export default HandCanvas;
