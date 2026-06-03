# EchoGlove V4 Claude Code AI Programming Prompts
## For Sensor Architecture Migration: Hall→Flex + 6DOF IMU

**Version:** V4.0-MIGRATION  
**Target:** Claude Code (AI Programming Assistant)  
**Project:** EchoGlove-SLR-MOCAP-Alpha  
**Date:** 2026-05-31

---

## Prompt 00: Project Context & Constraints (Prepend to ALL prompts)

```
You are an expert embedded systems and full-stack AI engineer working on EchoGlove V4, 
a sign-language translation data glove. The project is migrating from V3 (TMAG5273 Hall sensors 
+ fingertip magnets) to V4 (ADS1115 ADC + 5× flex sensors + BNO085 6DOF IMU).

CRITICAL CONSTRAINTS:
- MCU: ESP32-S3-DevKitC-1 (8MB Flash, 8MB PSRAM, dual-core 240MHz)
- Framework: ESP-IDF (FreeRTOS), NOT Arduino
- Build: PlatformIO (pio run / pio run -t upload)
- Language: C for firmware, Python for relay, TypeScript/React for web, C# for Unity
- I2C: GPIO8(SDA), GPIO9(SCL), 400kHz
- Power: 3.3V rail, target total draw < 200mA
- Real-time: Core1 runs 100Hz sensor sampling; Core0 runs inference + UDP
- Protobuf: nanopb for firmware, protobuf-python for relay
- Existing code: Check /glove_firmware/, /glove_relay/, /glove_web/, /glove_unity/ directories

NEVER:
- Use Arduino APIs (Wire.h, etc.)
- Block FreeRTOS tasks > 10ms
- Use malloc in Core1 sampling task
- Use floating-point in ISRs
- Break existing WebSocket API contract without version bump

ALWAYS:
- Use ESP-IDF native APIs (i2c_driver_install, xTaskCreatePinnedToCore, etc.)
- Include error handling with ESP_ERROR_CHECK or explicit retry logic
- Add // V4-MIGRATION comments to all modified files
- Maintain backward compatibility where possible (versioned protobuf)
```

---

## Prompt 01: Phase 1 — ADS1115 Driver Implementation

```
CONTEXT:
We are migrating from TCA9548A I2C mux + TMAG5273 to 2× ADS1115 16-bit ADC modules 
for reading 5 flex sensors. The ADS1115s are on the same I2C bus at addresses 0x48 and 0x49.

HARDWARE:
- ADS1115 #1 (0x48): reads Flex Index (AIN0), Middle (AIN1), Ring (AIN2), Pinky (AIN3)
- ADS1115 #2 (0x49): reads Flex Thumb (AIN0), 3 spare channels
- Each flex sensor uses voltage divider: 3.3V → [Flex] → ADC_IN → [22kΩ] → GND
- 100nF cap from ADC_IN to GND per channel
- I2C: 400kHz, 4.7kΩ pull-ups

TASK:
Create a complete ESP-IDF driver for ADS1115 in /glove_firmware/src/drivers/ads1115.h and .c

REQUIREMENTS:
1. Support multiple instances via I2C address (0x48, 0x49)
2. Configure for continuous conversion mode, 860SPS, single-ended inputs
3. PGA set to ±4.096V (sufficient for 3.3V divider output)
4. Provide API:
   - ads1115_init(i2c_port, i2c_addr, ads1115_handle_t* out_handle)
   - ads1115_read_channel(ads1115_handle_t handle, uint8_t channel, int16_t* out_raw)
   - ads1115_set_channel(ads1115_handle_t handle, uint8_t channel) // for continuous mode switching
5. Internal register map constants (CONFIG, CONVERSION, etc.)
6. Handle I2C NACK with 3 retries and exponential backoff
7. Thread-safe for FreeRTOS (use mutex if shared I2C bus)

TEST:
Write a simple test app in /glove_firmware/test/test_ads1115.c that:
- Initializes both ADS1115 modules
- Reads all 5 channels at 100Hz for 10 seconds
- Prints raw ADC values to console
- Validates readings are in expected range (0–32767)

OUTPUT:
- ads1115.h
- ads1115.c
- test_ads1115.c
- Update platformio.ini if new dependencies needed
```

---

## Prompt 02: Phase 1 — BNO085 6DOF Mode Configuration

```
CONTEXT:
V3 used BNO085 in 9DOF mode (SH2_ROTATION_VECTOR) which uses magnetometer. 
V4 must use 6DOF mode (SH2_GAME_ROTATION_VECTOR) to avoid magnetic interference.
The BNO085 is at I2C address 0x4A.

TASK:
Modify the existing BNO085 driver in /glove_firmware/src/drivers/bno085.h and .c to:

REQUIREMENTS:
1. At initialization, explicitly DISABLE all magnetometer-based reports:
   - SH2_ROTATION_VECTOR (0x05)
   - SH2_GEOMAGNETIC_ROTATION_VECTOR (0x09)
   - SH2_MAGNETIC_FIELD (0x0E)
2. Enable ONLY SH2_GAME_ROTATION_VECTOR (0x08) at 100Hz (10ms interval)
3. Ensure quaternion output is in Q15 format (int16 / 32768.0 = float)
4. Add a compile-time flag: BNO085_USE_6DOF (default true in V4)
5. If BNO085_USE_6DOF is false, fall back to 9DOF for backward compatibility (V3 legacy)
6. Add getter: bno085_get_game_rotation_vector(int16_t* qw, int16_t* qx, int16_t* qy, int16_t* qz)
7. Validate that magnetometer accuracy field is 0 or unavailable in 6DOF mode

TEST:
Write /glove_firmware/test/test_bno085_6dof.c that:
- Initializes BNO085 in 6DOF mode
- Reads 1000 quaternions at 100Hz
- Checks that magnetometer accuracy is NOT reported (or is 0)
- Prints quaternion stability (std dev of w component over 10 seconds static)
- Validates std dev < 0.01 (stable)

OUTPUT:
- Modified bno085.h / bno085.c
- test_bno085_6dof.c
- Document Yaw drift rate in comments (expected ~5-10°/min)
```

---

## Prompt 03: Phase 2 — Flex Sensor Module & Calibration

```
CONTEXT:
We need a high-level module that abstracts 5 flex sensors, handles calibration, 
filtering, and normalization to 0-100% bend percentage.

HARDWARE:
- 5× Spectra Symbol 2.2" flex sensors via ADS1115
- Thumb: ADS1115 #2, AIN0
- Index: ADS1115 #1, AIN0
- Middle: ADS1115 #1, AIN1
- Ring: ADS1115 #1, AIN2
- Pinky: ADS1115 #1, AIN3

TASK:
Create /glove_firmware/src/sensors/flex_sensor.h and .c

REQUIREMENTS:
1. Struct flex_sensor_config_t mapping each finger to (ads1115_handle, channel)
2. API:
   - flex_sensor_init(flex_sensor_config_t* configs, uint8_t count)
   - flex_sensor_calibrate_open_hand() // blocking 3s average
   - flex_sensor_calibrate_fist() // blocking 3s average
   - flex_sensor_read_all(uint8_t* out_bend_pct_5) // 0-100 per finger
   - flex_sensor_save_calibration_to_nvs()
   - flex_sensor_load_calibration_from_nvs()
3. Filtering per channel:
   - EMA: y[n] = α * x[n] + (1-α) * y[n-1], α = 0.15
   - Deadband: ignore changes < 1% (1 ADC count in 0-100 scale)
   - Clamp: 0–100
4. Calibration storage in NVS (Non-Volatile Storage):
   - Key: "flex_cal_v4"
   - Value: raw_min[5], raw_max[5], crc32
5. Handle sensor swap (left/right hand) via compile flag: GLOVE_HAND_LEFT / GLOVE_HAND_RIGHT

TEST:
Write /glove_firmware/test/test_flex_sensor.c that:
- Simulates ADC values (inject test pattern)
- Verifies calibration normalization
- Tests EMA filter step response
- Tests deadband behavior
- Tests NVS save/load roundtrip

OUTPUT:
- flex_sensor.h
- flex_sensor.c
- test_flex_sensor.c
```

---

## Prompt 04: Phase 2 — Remove V3 Hall Stack & Update Build

```
CONTEXT:
V3 files that must be REMOVED or DEPRECATED:
- /glove_firmware/src/drivers/tmag5273.h, .c
- /glove_firmware/src/drivers/tca9548a.h, .c
- /glove_firmware/src/sensors/hall_sensor.h, .c (if exists)
- References to magnetometer in Kalman filter (if magnet-specific)

TASK:
1. Delete tmag5273 and tca9548a drivers entirely
2. Update /glove_firmware/src/CMakeLists.txt (or component.mk) to remove deleted files
3. Update /glove_firmware/platformio.ini:
   - Remove any TMAG5273-related lib_deps
   - Add ADS1115 dependency if using external library (or keep self-contained)
4. Update /glove_firmware/src/main.c (or app_main.c):
   - Remove tmag5273_init() and tca9548a_init() calls
   - Add ads1115_init() for both modules
   - Add flex_sensor_init()
   - Add bno085_init() with 6DOF flag
5. Update Kconfig (if used) to remove Hall sensor options, add Flex sensor options
6. Search entire codebase for "TMAG5273", "TCA9548A", "tmag", "tca9548" and remove/replace
7. Ensure project builds cleanly: `pio run` succeeds with zero errors

BACKWARD COMPATIBILITY:
- Keep V3 protobuf schema file as sensor_frame_v3.proto (for reference)
- Add comment in main: "// V4 MIGRATION: Hall stack removed [DATE]"

OUTPUT:
- Clean build log (pio run output)
- List of all deleted/modified files
- Updated main.c initialization sequence
```

---

## Prompt 05: Phase 3 — Protobuf Schema V4 & Code Generation

```
CONTEXT:
The communication protocol uses nanopb on ESP32 and protobuf-python on relay.
V4 schema changes the sensor payload from 5×3D magnetometer to 5×1D flex percentage.

TASK:
1. Create /glove_firmware/proto/sensor_frame_v4.proto:

```protobuf
syntax = "proto3";

message SensorFrameV4 {
  uint32 timestamp_ms = 1;

  // Flex sensors: 0-100% bend (uint8 sufficient, but use uint32 for nanopb compatibility)
  uint32 flex_thumb = 2;
  uint32 flex_index = 3;
  uint32 flex_middle = 4;
  uint32 flex_ring = 5;
  uint32 flex_pinky = 6;

  // BNO085 6DOF Quaternion (Q15 fixed-point)
  sint32 quat_w = 7;
  sint32 quat_x = 8;
  sint32 quat_y = 9;
  sint32 quat_z = 10;

  uint32 seq_num = 11;
  uint32 battery_mv = 12;
}
```

2. Generate nanopb C code:
   - sensor_frame_v4.pb.h
   - sensor_frame_v4.pb.c
3. Update Python relay parser:
   - /glove_relay/proto/sensor_frame_v4.proto (same file)
   - Generate with `protoc --python_out=...`
   - Update /glove_relay/src/parser.py to parse V4 messages
4. Add version detection:
   - First byte of UDP payload = 0x04 (V4 version marker)
   - Relay auto-detects V3 vs V4 and routes to correct parser
5. Update Nanopb encoding in firmware:
   - /glove_firmware/src/comm/protobuf_tx.c: encode SensorFrameV4 instead of V3
   - Map flex_sensor_read_all() output to protobuf fields
   - Map bno085_get_game_rotation_vector() to quat fields

TEST:
- Encode a V4 frame on ESP32, decode in Python relay
- Verify all 12 fields round-trip correctly
- Measure encoded size: should be ~30-35 bytes

OUTPUT:
- sensor_frame_v4.proto
- Generated .pb.h / .pb.c
- Updated parser.py
- Updated protobuf_tx.c
- Version detection logic
```

---

## Prompt 06: Phase 3 — Core1 Sampling Task Rewrite

```
CONTEXT:
Core1 runs the 100Hz sensor sampling task. V3 used I2C mux + Hall polling.
V4 uses direct ADS1115 + BNO085 I2C reads.

TASK:
Rewrite /glove_firmware/src/tasks/sampling_task.c

REQUIREMENTS:
1. Task config: pinned to Core1, priority 10, stack 4096 bytes
2. Loop rate: exactly 100Hz (10ms period), use vTaskDelayUntil()
3. Sequence per iteration:
   a. Read ADS1115 #1 channels 0-3 (4 reads, ~2ms total at 860SPS)
   b. Read ADS1115 #2 channel 0 (1 read, ~0.5ms)
   c. Apply EMA filter to all 5 raw values
   d. Normalize to 0-100% using calibration table
   e. Read BNO085 Game Rotation Vector (int16 quat[4])
   f. Assemble SensorFrameV4 protobuf
   g. Push to RingBuffer (FreeRTOS queue)
4. Timing budget: entire loop < 8ms (leave 2ms margin)
5. If any sensor read fails (I2C NACK, timeout), retry once; if still fail, 
   send last valid data with error flag bit in seq_num (MSB)
6. Add runtime statistics:
   - min/max/avg loop duration (reported every 1000 loops)
   - missed deadline counter

PERFORMANCE TARGET:
- Jitter < 0.5ms (std dev of loop period)
- CPU usage on Core1 < 70%

TEST:
Write /glove_firmware/test/test_sampling_task.c that:
- Runs sampling task for 10 seconds
- Measures actual loop frequency (should be 100.0 ± 0.5 Hz)
- Measures max loop duration (should be < 8ms)
- Verifies RingBuffer receives exactly 1000 frames in 10s

OUTPUT:
- sampling_task.c (complete rewrite)
- sampling_task.h
- test_sampling_task.c
- Performance log snippet
```

---

## Prompt 07: Phase 4 — Python Relay Protobuf Parser Update

```
CONTEXT:
The Python relay receives UDP packets from ESP32, parses Protobuf, and forwards 
JSON via WebSocket. V4 changes the payload format.

TASK:
Update /glove_relay/src/parser.py and related files.

REQUIREMENTS:
1. Auto-detect V3 vs V4 packets:
   - Read first byte: 0x03 = V3, 0x04 = V4
   - Route to appropriate parser
2. V4 parser:
   - Decode SensorFrameV4 protobuf
   - Convert Q15 quaternions to float: q_f = q_i16 / 32768.0
   - Convert flex percentages to float (0.0–100.0)
   - Validate ranges (flex 0-100, quat magnitude ~1.0)
   - Add timestamp_ms to server clock if drift > 100ms
3. JSON output schema (WebSocket):
```json
{
  "version": 4,
  "timestamp_ms": 123456,
  "seq_num": 789,
  "flex": {
    "thumb": 45.2,
    "index": 12.0,
    "middle": 88.5,
    "ring": 67.3,
    "pinky": 5.0
  },
  "quaternion": {
    "w": 0.987,
    "x": 0.091,
    "y": -0.123,
    "z": 0.045
  },
  "battery_mv": 3700
}
```
4. Add logging: log flex values at DEBUG level, quat at TRACE level
5. Add metrics: Prometheus-style counter for packets_parsed_v4_total, errors_total

TEST:
Write /glove_relay/tests/test_parser_v4.py:
- Encode V4 protobuf in Python (same as firmware)
- Feed through parser
- Assert JSON output matches expected values
- Test auto-detection with mixed V3/V4 stream

OUTPUT:
- Updated parser.py
- test_parser_v4.py
- Updated requirements.txt if new protobuf version needed
```

---

## Prompt 08: Phase 5 — Web Frontend R3F Skeleton Update

```
CONTEXT:
The React + R3F web frontend renders a 3D hand skeleton in real-time.
V3 used magnetometer-based inverse kinematics. V4 uses direct flex percentage 
mapped to joint angles.

TASK:
Update /glove_web/src/components/HandSkeleton.tsx (or equivalent)

REQUIREMENTS:
1. Parse new WebSocket JSON (version 4)
2. Map flex% to finger bend angles (degrees → radians):
   - Thumb: 0-100% → 0°–70° (metacarpophalangeal + interphalangeal combined)
   - Index: 0-100% → 0°–90°
   - Middle: 0-100% → 0°–90°
   - Ring: 0-100% → 0°–90°
   - Pinky: 0-100% → 0°–80°
3. Apply rotation to finger bone hierarchy:
   - Each finger has 3 bones: proximal, middle, distal
   - Distribute total bend angle: proximal 50%, middle 30%, distal 20%
   - Example: index 90° bend → proximal 45°, middle 27°, distal 18°
4. Palm orientation: apply quaternion directly to wrist/palm group
5. Add visual indicator for flex values (optional HUD bar per finger)
6. Add calibration button in UI: "Calibrate Open Hand" / "Calibrate Fist"
   - Sends command via WebSocket to ESP32 to trigger calibration
7. Maintain V3 backward compatibility: if version==3, use old IK path

TEST:
- Render hand with all flex=0 (open hand) → visually flat
- Render hand with all flex=100 (fist) → visually closed
- Test quaternion rotation with known orientation
- Verify 60 FPS with 100Hz WebSocket updates

OUTPUT:
- Updated HandSkeleton.tsx
- Updated useWebSocket hook (if schema changes)
- Calibration UI component
- Storybook/test if available
```

---

## Prompt 09: Phase 6 — Dataset Collection Web Tool

```
CONTEXT:
V4 requires complete dataset recollection because sensor modality changed.
We need a web-based tool for volunteers to record labeled gesture data.

TASK:
Create /glove_web/src/tools/DataCollector.tsx (or standalone page)

REQUIREMENTS:
1. UI Flow:
   a. Subject info: name, age, gender, hand dominance, hand length
   b. Calibration: "Hold open hand" (3s countdown) → "Make fist" (3s countdown)
   c. Gesture recording: 46 CSL classes, 20 reps each
   d. Real-time preview: 3D hand + flex bars + current gesture label
   e. Review & delete: allow replay and discard bad samples
2. Data format per sample:
```json
{
  "subject_id": "S001",
  "gesture_label": "hello_csl",
  "rep_num": 1,
  "timestamp_ms": 123456789,
  "samples": [
    {"flex": [45,12,88,67,5], "quat": [0.987,0.091,-0.123,0.045], "t": 0},
    ... // 100Hz, ~2s duration = ~200 frames
  ],
  "calibration": {"open": [raw5], "fist": [raw5]}
}
```
3. Export: JSONL or TFRecord format for model training
4. Backend: store in browser IndexedDB; export to file; optional upload to server
5. Quality checks:
   - Minimum flex range per gesture (>20% variation)
   - No dropped frames (>5% missing = reject)
   - Visual confirmation: 3D replay matches recorded gesture

TEST:
- Record 5 gestures × 3 reps
- Export and verify JSONL structure
- Check frame count consistency

OUTPUT:
- DataCollector.tsx (or full page)
- dataExporter.ts utility
- IndexedDB schema
- README for data collection protocol
```

---

## Prompt 10: Phase 7 — L1 Model Architecture Update (PyTorch)

```
CONTEXT:
V3 L1 model used 1D-CNN+Attention with 19-dim input (5×3D mag + 4 quat).
V4 input is 9-dim (5 flex + 4 quat). Model must be redesigned for smaller input.

TASK:
Update /glove_relay/models/l1_cnn_attention.py

REQUIREMENTS:
1. Input: (batch, seq_len, 9) where 9 = [flex_thumb, flex_index, flex_middle, flex_ring, flex_pinky, quat_w, quat_x, quat_y, quat_z]
2. Architecture:
   - Conv1D: 9 → 32 channels, kernel=3, stride=1, padding=1
   - ReLU + BatchNorm1d(32)
   - MaxPool1d(2)
   - Conv1D: 32 → 32 channels, kernel=3
   - ReLU + BatchNorm1d(32)
   - Self-Attention: embed_dim=32, num_heads=2, dropout=0.1
   - Flatten + FC: 32*seq_len/2 → 64 → 46 classes
   - Dropout(0.3) between FC layers
3. Output: 46-class softmax (CSL gestures)
4. Constraints:
   - FLOPs < 50M (for ESP32-S3 inference compatibility)
   - Model size < 200KB (quantized INT8)
   - Sequence length: 20 frames (200ms at 100Hz)
5. Training config:
   - Loss: CrossEntropy with label smoothing (0.1)
   - Optimizer: AdamW, lr=1e-3, weight_decay=1e-4
   - Scheduler: CosineAnnealing, T_max=200
   - Epochs: 200 with early stopping (patience=20)
   - Augmentation: Gaussian noise on flex (σ=2%), quaternion rotation augmentation
6. Export: ONNX → TensorFlow Lite (INT8 quantization) for ESP32-S3

TEST:
- Train on synthetic random data (sanity check)
- Verify output shape (batch, 46)
- Verify FLOPs count with ptflops or similar
- Verify model exports to TFLite successfully

OUTPUT:
- l1_cnn_attention_v4.py
- train_l1_v4.py script
- export_tflite_v4.py
- Updated model_pool.yaml with V4 model entry
```

---

## Prompt 11: Phase 7 — ST-GCN L2 Model Update

```
CONTEXT:
V2 (Relay) uses ST-GCN for complex gesture sequences. Node features changed from 19-dim to 9-dim.

TASK:
Update /glove_relay/models/l2_st_gcn.py

REQUIREMENTS:
1. Hand skeleton graph (unchanged topology):
   - Nodes: 6 (wrist, thumb_tip, index_tip, middle_tip, ring_tip, pinky_tip)
   - Edges: palm→each fingertip (5 edges) + adjacent fingers (4 edges)
   - Self-loops included
2. Node feature assignment:
   - Wrist node: [0, 0, 0, 0, 0, qw, qx, qy, qz] (flex zeros, full quat)
   - Thumb node: [flex_thumb, 0, 0, 0, 0, qw, qx, qy, qz]
   - Index node: [0, flex_index, 0, 0, 0, qw, qx, qy, qz]
   - ... etc for each finger
   - Rationale: quat is global (palm), flex is local (finger)
3. ST-GCN layers: 3 layers, 64 channels, kernel_size=(3, 1) temporal×spatial
4. Temporal kernel: 3 frames (30ms)
5. Output: 46 classes + "background" class
6. Input: (batch, 9, 6, T) where 9=features, 6=nodes, T=sequence length (e.g., 60 frames = 600ms)

TEST:
- Forward pass with random tensor of correct shape
- Verify output shape (batch, 46)
- Verify graph adjacency matrix is symmetric

OUTPUT:
- l2_st_gcn_v4.py
- graph.py (hand skeleton adjacency matrix)
- train_l2_v4.py
```

---

## Prompt 12: Phase 8 — Unity ms-MANO Hand Model Update

```
CONTEXT:
Unity Pro uses ms-MANO for high-fidelity hand rendering. V4 changes data source.

TASK:
Update /glove_unity/Scripts/HandPoseDriver.cs

REQUIREMENTS:
1. WebSocket client receives V4 JSON
2. Map flex% to MANO joint angles:
   - MANO has 16 joints (per hand)
   - Map: flex% → joint_0 (base) → joint_1 (mid) → joint_2 (tip)
   - Angle distribution: base 50%, mid 30%, tip 20% of total flex%
3. Quaternion application:
   - Apply palm quat to wrist transform
   - Use LookRotation or direct quaternion multiplication
4. Add calibration mode:
   - Press 'C' to calibrate: records current flex as "open hand" baseline
   - Press 'F' to set "fist" baseline
   - All subsequent flex% computed relative to these baselines
5. Drift compensation:
   - Every 5 seconds, if hand is stationary (flex variation < 2%), 
     reset palm forward vector to current orientation
   - This handles 6DOF Yaw drift gracefully
6. Smoothing: apply 5-frame moving average to joint angles for visual stability

TEST:
- Play mode with mock V4 data stream
- Verify open hand pose (all flex=0) matches MANO rest pose
- Verify fist pose (all flex=100) matches MANO fist pose
- Check frame rate > 90 FPS on target hardware

OUTPUT:
- HandPoseDriver.cs
- CalibrationManager.cs
- MockDataGenerator.cs (for testing without glove)
```

---

## Prompt 13: Phase 9 — End-to-End Integration Test

```
CONTEXT:
Final validation of full V4 pipeline: glove → firmware → relay → web/unity.

TASK:
Create integration test suite in /tests/integration/v4/

REQUIREMENTS:
1. Hardware-in-the-loop test:
   - Connect actual ESP32 + ADS1115 + BNO085 + 5 flex sensors
   - Run for 5 minutes, 100Hz
   - Verify: packet loss < 0.1%, no I2C errors, no FreeRTOS watchdog triggers
2. Latency test:
   - Measure t1: sensor physical bend → t2: UDP packet → t3: WebSocket JSON → t4: R3F render
   - Target: t4-t1 < 100ms (end-to-end)
   - Log: min/avg/max/p95/p99 latency
3. Accuracy test:
   - Perform 10 known gestures (subset of 46)
   - Compare L1 prediction vs ground truth
   - Target: Top-1 > 90% (requires trained model; use mock if unavailable)
4. Drift test:
   - Place glove static for 5 minutes
   - Measure palm orientation drift (Yaw)
   - Target: < 15° total drift in 5min
5. Battery test:
   - Full battery (4.2V) to cutoff (3.3V)
   - Target: > 2 hours continuous operation
6. Stress test:
   - Rapid open/close fist at 2Hz for 1 minute
   - Verify no missed frames, no flex sensor saturation

TEST OUTPUT:
- /tests/integration/v4/report.md with all metrics
- /tests/integration/v4/latency_histogram.png (generated from logs)
- Pass/fail summary

OUTPUT:
- integration_test.py (orchestrator)
- test_cases/ directory
- report_template.md
- README for running integration tests
```

---

## Prompt 14: Phase 9 — Documentation & Release

```
CONTEXT:
V4 migration is complete. Need to update all documentation for public release.

TASK:
1. Update /README.md:
   - Replace V3 architecture diagram with V4
   - Update BOM table
   - Update Quick Start (hardware wiring changed)
   - Add "Migration from V3" section
2. Update /docs/HARDWARE.md:
   - Full wiring diagram for ADS1115 + flex + BNO085
   - Fritzing or KiCad schematic
   - PCB layout recommendations (analog traces short, away from digital)
3. Update /docs/FIRMWARE.md:
   - New driver architecture
   - Calibration procedure
   - Filter parameters
4. Update /docs/MODEL.md:
   - V4 input features (9-dim)
   - Training from scratch instructions
   - Dataset download link (when available)
5. Update /docs/API.md:
   - WebSocket JSON schema V4
   - Protobuf binary schema V4
   - Version detection protocol
6. Update /CHANGELOG.md:
   - V4.0.0: [DATE] Migrated from Hall+Magnet to Flex+6DOF IMU
   - List breaking changes
   - Migration guide for V3 users
7. Tag release: git tag v4.0.0

OUTPUT:
- All updated markdown files
- CHANGELOG.md
- Release notes draft
```

---

## Prompt 15: Optional — Fallback All-IMU Upgrade Path

```
CONTEXT:
If V4 Flex+6DOF fails to meet accuracy targets (e.g., < 85% Top-1), 
we need a documented upgrade path to all-IMU (similar to T-800 / MANUS).

TASK:
Create /docs/ROADMAP_ALL_IMU.md — a design document for future V5.

REQUIREMENTS:
1. Sensor config: 11× BHI360 (or BMI270 + BMM150) — 1 per finger segment + palm
   - Thumb: 2 IMU (metacarpal + phalange)
   - Each finger: 2 IMU (proximal + distal)
   - Palm: 1 IMU
2. Communication: SPI + I2C hybrid, or RS-485 cascaded
3. Calibration: T-pose + N-pose auto-calibration (like Xsens)
4. Drift correction: Magnetic anchor on forearm (far from hand, stable reference)
5. Cost estimate: ~$80-120 BOM
6. Complexity: High; recommend only if V4 fails after 3 months optimization

OUTPUT:
- ROADMAP_ALL_IMU.md
- Block diagram
- Cost analysis spreadsheet
```

---

*End of Claude Code Prompts V4. Execute in order, with Gate Review after Phase 1.*
