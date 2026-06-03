# EchoGlove V5.0 DualGloveFlex — Design Specification

> **Version**: V5.0  
> **Date**: 2026-06-01  
> **Status**: Draft — Pending user review  
> **Supersedes**: V3 (Hall sensor architecture)  
> **Reason for migration**: Magnetic bead interference with TMAG5273 Hall sensors and BNO085 magnetometer makes V3 physically unworkable.

---

## 1. System Overview

### 1.1 Architecture

3-tier, 3×ESP32-S3 architecture with dual-hand support for sign language translation and hand motion capture.

```
┌─────────────────────────────────────────────────────────────────────┐
│                     EchoGlove V5.0 System Architecture              │
│                                                                     │
│  ┌──────────────┐    ESP-NOW     ┌──────────────┐                   │
│  │  Left Glove  │  ──────────►   │   Receiver   │                   │
│  │  ESP32-S3    │   (2ms,69B)    │   ESP32-S3   │                   │
│  │  11-dim      │               │   28-dim      │                   │
│  │  Tier1 CNN   │               │   Tier2 Attn  │                   │
│  └──────────────┘               │   + Pairing   │                   │
│                                 └──────┬───────┘                   │
│  ┌──────────────┐    ESP-NOW            │ USB Serial                │
│  │ Right Glove  │  ──────────►   ┌──────┘  (Protobuf)              │
│  │  ESP32-S3    │               │                                   │
│  │  11-dim      │               ▼                                   │
│  │  Tier1 CNN   │        ┌──────────────┐    WS:8765  ┌──────────┐ │
│  └──────────────┘        │  PC Relay    │ ──────────► │ React3F  │ │
│                          │  Tier3 L1/L2 │             │ Unity XR │ │
│                          │  NLP + TTS   │             └──────────┘ │
│                          └──────────────┘                          │
└─────────────────────────────────────────────────────────────────────┘
```

### 1.2 Core Data Flow

1. Each glove: 5×Flex + BNO085 → 11-dim @ 100Hz → Kalman filter → Tier1 inference → ESP-NOW broadcast
2. Receiver: Pair left/right frames by tick_id → compute 6-dim relative features → 28-dim → Tier2 inference → USB forward to PC
3. PC Relay: Tier3 inference (L1 classification + L2 ST-GCN) → Confidence Router → NLP → TTS → WebSocket
4. Frontend: React3F / Unity XR Hands renders 26-joint virtual hands

### 1.3 Feature Dimensions

| Scope | Dimensions | Composition |
|-------|-----------|-------------|
| Single hand | 11 | Flex[5] + Euler[3] + Gyro[3] |
| Dual hand (raw) | 22 | Left[11] + Right[11] |
| Relative features | 6 | ΔEuler[3] + ΔQuatDist[1] + ΔGyroNorm[1] + ΔGyroAxis[1] |
| Dual hand (full) | 28 | Left[11] + Right[11] + Relative[6] |

### 1.4 Hot-Swap Between Tiers

- 5-frame linear blend (~50ms transition at 100Hz)
- Formula: `output(t) = (1-α) * old_tier + α * new_tier`, α ramps 0→1
- Temperature calibration aligns softmax distributions across tiers

### 1.5 Confirmed Design Decisions

| # | Decision | Choice | Rationale |
|---|----------|--------|-----------|
| D1 | Flex sensor | SpectraFlex 2.2" (domestic for dev, Spectra Symbol for final) | Cost-effective development, production-grade final |
| D2 | Pull-down resistor | 47kΩ | Per Mimo V5.1 recommendation, optimal voltage divider range |
| D3 | Feature dimensions | 11 per hand (5 flex + 3 euler + 3 gyro) | Minimum viable for gesture classification |
| D4 | ST-GCN nodes | Hierarchical: 12-node (Tier1/2) + 42-node (Tier3) | Reduces edge compute, maintains accuracy where it matters |
| D5 | Gesture classes | 46 (MVP) | ASL alphabet + digits + special gestures |
| D6 | Protobuf | New schema v5 + version field | Clean break from V3 Hall-based schema |
| D7 | I2C topology | Flat bus (no MUX) | No address conflicts: BNO085@0x4B + ADS1115@0x48 + ADS1115@0x49 |
| D8 | Calibration | 6s (3s open hand + 3s fist) | Sufficient for 5-point piecewise linear calibration |
| D9 | Receiver MCU | Yes, 3×ESP32-S3 architecture | Enables Tier2 inference when PC is offline |
| D10 | ST-GCN graph | 12-node (T1/2) + 42-node (T3) hierarchical | Mimo V5.1 recommendation |
| D11 | Tier2 model | Gated Bi-CrossAttn only (~80KB) | MS-TCN too heavy for ESP32-S3 real-time |
| D12 | Frontend | React3F MVP + Unity XR Hands parallel | Fast validation + XR device support |
| D13 | NLP/CTC | Phased: isolated gestures first, CTC later | Reduces Phase 1 complexity |
| D14 | Data pipeline | CSV + Python training | Simple, proven, no platform dependency |
| D15 | Migration strategy | New branch + Simulation-first execution | End-to-end validation in week 1 |

---

## 2. Hardware Layer

### 2.1 BOM (Per Glove)

| Component | Model | Qty | Interface | Notes |
|-----------|-------|-----|-----------|-------|
| MCU | ESP32-S3-DevKitC-1 N16R8 | 1 | - | 8MB Flash + 8MB PSRAM |
| IMU | GY-BNO085 | 1 | I2C 0x4B | Game Rotation Vector mode (6DOF) |
| ADC | ADS1115 (4ch 16-bit) | 2 | I2C 0x48, 0x49 | 2-3 flex sensors per ADC |
| Flex sensor | SpectraFlex 2.2" | 5 | Analog → ADS1115 | Domestic alternative for dev |
| Pull-down resistor | 47kΩ | 5 | - | Voltage divider for flex sensors |
| Wiring | Breadboard + Dupont | - | - | Development phase |

### 2.2 Receiver

| Component | Model | Qty | Notes |
|-----------|-------|-----|-------|
| MCU | ESP32-S3-DevKitC-1 N16R8 | 1 | Data aggregation + Tier2 inference |
| USB cable | USB-C | 1 | Connection to PC |

### 2.3 I2C Topology (Flat Bus, No MUX)

```
ESP32-S3 (SDA=GPIO8, SCL=GPIO9, 100kHz)
  ├── BNO085      @ 0x4B
  ├── ADS1115 #1  @ 0x48  (Ch0=Thumb, Ch1=Index, Ch2=Middle)
  └── ADS1115 #2  @ 0x49  (Ch3=Ring, Ch4=Pinky)
```

### 2.4 Flex Sensor Wiring (Per Channel)

```
3.3V ──┤Flex Sensor├── ADC Input ──┤47kΩ├── GND
```

### 2.5 ADS1115 Configuration

- Gain: ±4.096V (PGA=1), covers 0-3.3V flex sensor range
- Sample rate: 860 SPS per channel; 4 channels polled ≈ 215Hz/channel, sufficient for 100Hz
- Data format: 16-bit signed → normalize to [0, 1]

### 2.6 ESP-NOW Communication

- Left/right gloves broadcast mode, no pairing required
- Receiver broadcasts SYNC_TICK every 10ms; gloves use received tick_id for frame alignment
- Sync precision: <2ms (ESP-NOW ~1ms + sampling jitter ~1ms)

---

## 3. Firmware Layer

### 3.1 FreeRTOS Task Allocation (Per Glove)

| Task | Core | Priority | Freq | Purpose |
|------|------|----------|------|---------|
| Task_SensorRead | Core 1 | 3 (highest) | 100Hz | I2C read ADS1115×2 + BNO085, Kalman filter |
| Task_Inference | Core 0 | 2 | ~30Hz | Tier1 CNN inference, output gesture_id + confidence |
| Task_Comms | Core 0 | 1 | 100Hz | ESP-NOW send + BLE provisioning |
| Task_Watchdog | Core 0 | 0 (lowest) | 1Hz | Watchdog feed, health check |

### 3.2 Data Structures (`data_structures.h`)

```cpp
// V5 constants — all Hall-related constants REMOVED
#define NUM_FLEX_SENSORS     5
#define IMU_FEATURE_COUNT    6     // 3 euler + 3 gyro
#define SINGLE_HAND_FEATURES (NUM_FLEX_SENSORS + IMU_FEATURE_COUNT)  // 11
#define DUAL_HAND_FEATURES   (2 * SINGLE_HAND_FEATURES + 6)          // 28
#define WINDOW_SIZE          30
#define NUM_CLASSES          46
#define SENSOR_RATE_HZ       100

struct SensorData {
    float flex[NUM_FLEX_SENSORS];     // 5 flex sensor normalized values
    float quaternion[4];               // BNO085 quaternion (w,x,y,z)
    float euler[3];                    // Derived from quaternion (roll,pitch,yaw)
    float gyro[3];                     // Angular velocity (deg/s)
    uint32_t timestamp_us;
    uint32_t seq;
    // toFeatureArray() → float[11] (flex + euler + gyro)
};

struct GestureResult {
    int32_t gesture_id;
    float confidence;
    float scores[NUM_CLASSES];         // 46 class probabilities
    bool valid;
    bool l2_requested;
};

struct GlovePacket {                    // 69 bytes, ESP-NOW broadcast
    uint8_t  magic[2];                  // {0x45, 0x47} "EG"
    uint8_t  version;                   // 5
    uint8_t  hand_id;                   // 0=left, 1=right
    uint32_t tick_id;                   // Receiver broadcast sync tick
    uint32_t timestamp_us;
    float    flex[5];
    float    imu[6];                    // euler[3] + gyro[3]
    uint16_t l1_gesture_id;
    float    l1_confidence;
    uint8_t  status;                    // STREAMING/CALIBRATING/ERROR
    uint16_t checksum;                  // CRC16
    uint8_t  reserved[2];
};
```

### 3.3 Firmware Modules

| Module | File | Status | Description |
|--------|------|--------|-------------|
| ADS1115Manager | `lib/Sensors/ADS1115Manager.h` | **New** | Dual ADS1115 driver, I2C 0x48+0x49 |
| FlexManager | `lib/Sensors/FlexManager.h` | **Rewrite** | Read from ADS1115, 5-point piecewise linear calibration |
| SensorManager | `lib/Sensors/SensorManager.h` | **Rewrite** | Remove Hall/MUX, flat I2C, BNO085@0x4B |
| ESP-NOW Comms | `lib/Comms/ESPNOWTransmitter.h` | **New** | Replace UDP, 69-byte broadcast packet |
| Tier1 Model | `lib/Inference/Tier1Model.h` | **Modify** | Input 11-dim, output 46 classes |

### 3.4 Retained Modules (Modified)

| Module | Change |
|--------|--------|
| KalmanFilter1D.h | Channel count: 21 → 11 |
| SlidingWindow.h | No change (30-frame ring buffer) |
| FeatureNormalizer.h | Calibration period: 2s → 6s |
| TFLiteModel.h | No change (TFLite Micro engine) |
| ModelRegistry.h | No change (hot-switching) |

### 3.5 Removed Modules

| Module | Reason |
|--------|--------|
| TMAG5273.h | Hall sensor driver — abandoned |
| TCA9548A.h | I2C MUX — no longer needed (flat bus) |

### 3.6 Simulation Mode Upgrade

- Generate 11-dim single-hand data (replaces 15-dim Hall data)
- Support 20 gesture signatures (retained from V3)
- Configurable left/right hand ID
- CSV output compatible with training pipeline

---

## 4. Communication Protocol

### 4.1 Three-Level Communication Chain

```
Glove → ESP-NOW → Receiver → USB Serial → PC Relay → WebSocket → Frontend
       (69B,2ms)            (Protobuf)      (JSON)
```

### 4.2 ESP-NOW (Glove → Receiver)

- 69-byte `GlovePacket` struct (defined in Section 3.2)
- Broadcast mode, no pairing
- Receiver broadcasts SYNC_TICK every 10ms
- Packet loss handling: linear interpolation for up to 3 consecutive dropped frames

### 4.3 Receiver Aggregation Logic

1. Receive left/right packets, pair by tick_id
2. Compute 6-dim relative features:
   - `ΔEuler[3]`: euler_L[i] - euler_R[i] for i in (roll, pitch, yaw) — raw orientation difference
   - `ΔQuatDist[1]`: 2 * arccos(|q_L · q_R|) — quaternion angular distance (scalar)
   - `ΔGyroNorm[1]`: ||gyro_L|| - ||gyro_R|| — overall angular speed difference
   - `ΔGyroAxis[1]`: argmax(|gyro_L - gyro_R|) / 3 — dominant differing axis index (normalized)
3. Output 28-dim feature vector
4. Forward via USB Serial (Protobuf encoded)

### 4.4 Protobuf Schema (v5)

```protobuf
syntax = "proto3";
package echoglove_v5;

message GloveData {
    uint32 version = 1;         // 5
    uint32 timestamp = 2;
    uint32 hand_id = 3;         // 0=left, 1=right
    repeated float flex = 4;    // [5]
    repeated float imu = 5;     // [6]
    uint32 l1_gesture_id = 6;
    float l1_confidence = 7;
    repeated float relative = 8; // [6] ΔEuler[3]+ΔQuatDist[1]+ΔGyroNorm[1]+ΔGyroAxis[1], filled by receiver
    uint32 tier2_gesture_id = 9;
    float tier2_confidence = 10;
    string status = 11;
}

message ReceiverPacket {
    uint32 version = 1;
    uint32 tick_id = 2;
    GloveData left = 3;
    GloveData right = 4;
    repeated float relative_features = 5; // [6]
}
```

### 4.5 WebSocket (PC → Frontend)

- JSON format, port 8765
- Fields: `tier`, `blend_alpha`, `left_hand`, `right_hand`, `relative`, `inference`, `nlp_text`

---

## 5. Relay Layer (Inference + Models + NLP)

### 5.1 Tier Architecture

| Tier | Location | Model | Input | Output | Size | Latency |
|------|----------|-------|-------|--------|------|---------|
| Tier1 | Glove ESP32 | CNN+SE-Attention | 11-dim single hand | 46-class probs | ~80KB int8 | <10ms |
| Tier2 | Receiver ESP32 | Gated Bi-CrossAttn | 28-dim dual hand | 46-class probs | ~80KB int8 | ~30ms |
| Tier3-L1 | PC GPU | Gated Bi-CrossAttn + MS-TCN 4-stage | 28-dim × 30 frames | 46-class probs | ~500KB FP32 | ~15ms |
| Tier3-L2 | PC GPU | ST-GCN(42 nodes) → MS-TCN 4-stage → CTC | 28-dim × 30 frames | Continuous sequence | ~3MB FP32 | ~15ms |

### 5.2 Gated Bi-CrossAttention

```python
# Left and right hand features attend to each other, gate controls fusion strength
attn_L = MultiHeadAttention(Q=feat_L, KV=feat_R)  # Left attends to right
attn_R = MultiHeadAttention(Q=feat_R, KV=feat_L)  # Right attends to left
gate_L = sigmoid(W @ [feat_L; attn_L] + b)
gate_R = sigmoid(W @ [feat_R; attn_R] + b)
fused_L = gate_L * attn_L + (1-gate_L) * feat_L
fused_R = gate_R * attn_R + (1-gate_R) * feat_R
```

Parameters: gate_proj = Linear(d_model * 2, d_model) = 8,256 params per hand

### 5.3 ST-GCN Graph Structure (42 Nodes)

- Each hand: 21 nodes (WRIST + 5 fingers × MCP/PIP/DIP/TIP)
- Left hand: nodes 0-20, Right hand: nodes 21-41
- Edges: 40 intra-hand (20 per hand skeleton tree) + 6 cross-hand (fingertip correspondence) + 42 self-loops = **88 total**
- Tier1/2: 12-node simplified graph (Wrist + 5 fingertips × 2 hands), 22 edges
- Tier3: 42-node full graph, 88 edges
- 12→42 node feature mapping for tier transitions

### 5.4 Confidence Router (Three-Tier Fusion)

```python
# Always run Tier1
tier1_result = run_tier1(features_11d)

# Run Tier2 when receiver is online
if receiver_online:
    tier2_result = run_tier2(features_28d)

# Run Tier3 when PC is online
if pc_online:
    tier3_l1 = run_tier3_l1(window_28d_30frames)
    tier3_l2 = run_tier3_l2(window_28d_30frames)  # CTC decode

# Hot-swap: 5-frame linear blend
output = (1-blend_alpha) * old_tier_result + blend_alpha * new_tier_result
```

### 5.5 NLP Pipeline (Phase 1: Isolated Gestures)

- Grammar corrector (existing 15 tests retained)
- Gesture ID sequence → Chinese/English text
- TTS speech synthesis (existing 13 tests retained)
- Phase 2: CTC beam search for continuous sign language

### 5.6 Relay Test Migration

- ~60% of 133 tests retainable (NLP, TTS, WebSocket, base framework)
- Must rewrite: protobuf_parser (new schema), stgcn_model (new graph), confidence_router (three-tier fusion)

---

## 6. Frontend

### 6.1 Dual Frontend Strategy

| Frontend | Purpose | Skeleton | Communication |
|----------|---------|----------|---------------|
| React3F (Web) | MVP validation, debug tools | 21-keypoint MediaPipe topology | WebSocket JSON |
| Unity XR Hands | Final product, XR device support | 26-joint OpenXR standard | WebSocket/UDP JSON |

### 6.2 React3F MVP (Retain + Upgrade)

- Existing 21-keypoint 3D hand scaffold
- Upgrade: dual-hand rendering (left/right independent objects)
- Debug overlay: raw sensor values, tier selection, confidence

### 6.3 Unity XR Hands Integration

- Unity XR Hands 1.7 + OpenXR standard
- 26-joint skeleton (strictly per OpenXR spec):
  ```
  Wrist → Palm
  ├── ThumbMetacarpal → ThumbProximal → ThumbDistal → ThumbTip (4 joints)
  ├── IndexMetacarpal → IndexProximal → IndexIntermediate → IndexDistal → IndexTip (5 joints)
  ├── Middle... (5 joints)
  ├── Ring... (5 joints)
  └── Little... (5 joints)
  ```

### 6.4 5-DoF Flex Sensor → 26 Joint Mapping

```csharp
// Special thumb treatment (CMC dual-DOF)
thumb.CMC_flexion  = flex[0] * 1.0f;
thumb.CMC_abduction = imu_gyro_y * 0.5f;  // BNO085 assist
thumb.MCP = flex[0] * 0.8f;
thumb.IP  = flex[0] * 0.6f;

// Other four fingers: linear coupling
for (i = 1..4) {
    finger[i].MCP = flex[i] * 1.0f;
    finger[i].PIP = flex[i] * 0.67f;
    finger[i].DIP = flex[i] * 0.5f;
    finger[i].TIP = flex[i] * 0.3f;  // Passive, follows
}

// Wrist: BNO085 quaternion
wrist.rotation = new Quaternion(
    imu_quat.x, imu_quat.y, imu_quat.z, imu_quat.w
);  // Unity uses (x,y,z,w) order
```

### 6.5 Future Upgrade Path

- Phase 2: MANO parametric hand model replaces linear coupling
- Phase 3: Sensor + vision multimodal fusion (Unity XR native hand tracking overlay)

---

## 7. Data Collection & Training Pipeline

### 7.1 Calibration Flow

1. Open hand 3 seconds → record 5 flex sensor ADC minimum values
2. Fist 3 seconds → record 5 flex sensor ADC maximum values
3. Build 5-point piecewise linear calibration table → normalize to [0, 1]

### 7.2 CSV Data Format

```csv
timestamp,hand_id,flex0,flex1,flex2,flex3,flex4,euler_x,euler_y,euler_z,gyro_x,gyro_y,gyro_z,gesture_id
1234567,0,0.12,0.85,0.78,0.65,0.23,12.3,-45.6,78.9,0.5,-1.2,0.3,15
```

### 7.3 Training Pipeline

```python
# Data loading
df = pd.read_csv("dataset/left_hand_46class.csv")
X = df[flex_cols + imu_cols].values  # (N, 11)
y = df["gesture_id"].values

# Tier1 model training (CNN+SE-Attention)
model = Tier1CNN(input_dim=11, num_classes=46)
train(model, X, y, epochs=100, batch_size=32)

# Export TFLite int8
export_tflite_int8(model, "tier1_model_int8.tflite")
```

### 7.4 Dataset Scale Target (Phase 1 MVP)

- 46 isolated gesture classes
- 30 samples per class × 2 people = 2,760 samples
- 3 seconds per sample × 100Hz = 300 frames
- Total: ~828,000 frames

### 7.5 Dual-Hand Data Collection

- Wear both gloves simultaneously
- Each gesture: operator performs synchronized left/right actions
- CSV includes hand_id column; training separates or merges by hand_id

---

## 8. Migration Execution Plan

### Phase 0: Branch & Cleanup (1 day)

- Create `V5-DualGloveFlex` branch from Beta repo
- Delete all Hall/TMAG5273/TCA9548A related code
- Update CLAUDE.md, PROGRESS.md
- Verify: existing relay test skeleton compiles

### Phase 1: Simulation + Data Structures (3 days)

- Rewrite `data_structures.h` (11-dim single hand)
- Rewrite `SensorManager` simulation mode (V5 format data)
- Create `ADS1115Manager` stub (simulation mode)
- Rewrite `FlexManager` stub
- New proto definition + nanopb generation
- Verify: simulation outputs 11-dim data → CSV readable

### Phase 2: Relay Pipeline Upgrade (5 days)

- Update `protobuf_parser.py` (new schema v5)
- Create `Tier1CNN` model (PyTorch, ~24K params)
- Upgrade `ConfidenceRouter` (three-tier fusion logic)
- Update `stgcn_model.py` (42-node graph)
- Update WebSocket JSON format
- Verify: simulated data → full relay pipeline → frontend JSON output, 133 tests regression pass

### Phase 3: Receiver Firmware (3 days)

- Create `receiver_firmware` directory
- ESP-NOW receive + tick_id pairing + relative feature calculation
- USB Serial Protobuf forwarding
- Tier2 Gated Bi-CrossAttn inference (TFLite Micro)
- Verify: two simulated gloves → receiver → PC relay

### Phase 4: Glove Firmware Sensor Layer (5 days)

- Create `ADS1115Manager` (real I2C driver)
- Rewrite `FlexManager` (ADS1115 read + 5-point calibration)
- Rewrite `SensorManager` (flat I2C: BNO085@0x4B + ADS1115×2)
- Kalman filter channel adjustment (21 → 11)
- ESP-NOW transmit module
- Verify: single glove hardware → sensor readings → CSV

### Phase 5: Data Collection + Model Training (7 days)

- Calibration tool (6s open hand + fist)
- 46-class gesture data collection (2 people × 30 samples)
- Tier1 model training + TFLite int8 export
- Tier2 model training + TFLite int8 export
- Tier3 model training (L1 classification + L2 ST-GCN)
- Verify: model accuracy Tier1 >80%, Tier2 >85%, Tier3 >90%

### Phase 6: Frontend Integration (5 days)

- React3F dual-hand rendering upgrade
- Unity XR Hands project setup
- WebSocket data integration
- 5-DoF → 26 joint mapping script
- Verify: real-time data → 3D virtual hand rendering

### Phase 7: End-to-End Integration (3 days)

- Full chain: glove → receiver → PC → frontend
- Hot-swap testing (Tier1 ↔ Tier2 ↔ Tier3)
- Latency testing (E2E < 100ms target)
- Stability testing (30-minute continuous run)

**Total: ~32 days (approximately 6 weeks)**

---

## 9. Reference Repositories & Papers

### Top 5 Direct References

1. **Unity XR Hands**: Dual-hand virtual display, XR interaction
2. **Smart-Sign-Language-Translator-Glove** (Gill003): Sensor calibration, ESP32 communication
3. **CASA0018-Gloves-Edge-AI** (ReikiC): ESP32 lightweight model deployment
4. **Nature 2024** (Stretchable glove): Flex sensor + IMU fusion, pose reconstruction
5. **PenSLR** (arXiv 2406.16388): End-to-end sign language recognition with CTC

### Additional References

- **AI4Bharat/OpenHands**: Sign language recognition toolkit, transfer learning concepts
- **ASL-DataGlove** (farhanfuadabir): Dataset format, multi-model experimentation
- **Hand-Tracking-Glove** (isurusasangaetam): I2C multiplexer patterns, Madgwick AHRS
- **SignThought** (fletcherjiang): Latent chain-of-thought for sign language translation
- **ms-mano-unity**: Musculoskeletal hand model with biomechanical constraints

---

## 10. Risk Register

| Risk | Impact | Mitigation |
|------|--------|------------|
| Flex sensor non-linearity | Medium | 5-point piecewise calibration, per-user calibration |
| BNO085 yaw drift (no magnetometer) | Low | Acceptable for relative hand posture; Game Rotation Vector mode |
| ESP-NOW packet loss | Medium | Linear interpolation for up to 3 dropped frames |
| ADS1115 I2C bus contention | Low | 100kHz bus, only 3 devices, no address conflicts |
| Training data insufficient | High | Start with 46 classes, expand incrementally |
| ESP32-S3 Tier2 inference too slow | Medium | Gated Bi-CrossAttn only (~80KB), no MS-TCN on edge |
| Unity XR Hands learning curve | Low | React3F MVP first, Unity in parallel |

---

## 11. Open Questions

None remaining — all 15 design decisions confirmed by user.
