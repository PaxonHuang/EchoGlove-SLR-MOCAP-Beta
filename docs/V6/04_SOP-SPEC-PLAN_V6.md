# EchoGlove V6.0 — LSM6DSV16X Migration Design Specification

> **Version**: V6.0
> **Date**: 2026-06-23
> **Status**: Draft — Pending user review
> **Supersedes**: V5.0 DualGloveFlex + V5.2 P4 Base Station
> **Branch**: V6-LSM6DSV16X (to be created from V5-DualGloveFlex)
> **Reason for migration**: BNO085 cost ($15-25) and supply chain risk → LSM6DSV16X ($2-4) with embedded SFLP fusion, same 6-axis output, drop-in SensorData compatibility.

---

## Table of Contents

1. [System Overview](#1-system-overview)
2. [Hardware Layer](#2-hardware-layer)
3. [Firmware Layer](#3-firmware-layer)
4. [Communication Protocol](#4-communication-protocol)
5. [Relay Layer (Inference + Models + NLP)](#5-relay-layer)
6. [Frontend](#6-frontend)
7. [Data Collection & Training Pipeline](#7-data-collection--training-pipeline)
8. [Migration Execution Plan](#8-migration-execution-plan)
9. [Reference Repositories & Papers](#9-reference-repositories--papers)
10. [Risk Register](#10-risk-register)
11. [Open Questions](#11-open-questions)
12. [Innovation Points](#12-innovation-points)

---

## 1. System Overview

### 1.1 Architecture

3-tier, dual-glove + P4 smart base station architecture for sign language translation and hand motion capture.

```
┌─────────────────────────────────────────────────────────────────────────┐
│                     EchoGlove V6.0 System Architecture                  │
│                                                                         │
│  ┌──────────────┐    ESP-NOW     ┌──────────────┐   UART 2Mbps         │
│  │  Left Glove  │  ──────────►   │  C6 Relay    │──────────►┌────────┐ │
│  │  ESP32-S3    │   (2ms,69B)    │  ESP32-C6    │           │  P4    │ │
│  │  11-dim      │               └──────────────┘           │ 400MHz │ │
│  │  Tier1 CNN   │                                          │ Tier2  │ │
│  └──────────────┘                                          │ LVGL   │ │
│                                                            │ TTS    │ │
│  ┌──────────────┐    ESP-NOW                                └───┬────┘ │
│  │ Right Glove  │  ──────────►   (same C6)                     │ USB  │
│  │  ESP32-S3    │                                               │ HS   │
│  │  11-dim      │                                               ▼      │
│  │  Tier1 CNN   │        ┌──────────────┐    WS:8765  ┌──────────┐    │
│  └──────────────┘        │  PC Relay    │ ──────────► │ React3F  │    │
│                          │  Tier3 L1/L2 │             │ Unity XR │    │
│                          │  NLP + TTS   │             └──────────┘    │
│                          └──────────────┘                             │
└─────────────────────────────────────────────────────────────────────────┘
```

### 1.2 Core Data Flow

1. Each glove: 5×Flex + LSM6DSV16X → 11-dim @ 100Hz → Kalman filter → Tier1 inference → ESP-NOW broadcast
2. C6: Receive L/R packets → UART relay to P4
3. P4: Pair by tick_id → compute 6-dim relative features → 28-dim → Tier2 inference → LVGL display + TTS audio
4. P4 → USB HS → PC Relay: Tier3 inference (L1 classification + L2 ST-GCN) → Confidence Router → NLP → TTS → WebSocket
5. Frontend: React3F / Unity XR Hands renders 26-joint virtual hands

### 1.3 Feature Dimensions (Unchanged from V5)

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
- Heartbeat: 100ms intervals. 3 missed = downgrade.

### 1.5 Confirmed Design Decisions

| # | Decision | Choice | Rationale |
|---|----------|--------|-----------|
| D1 | IMU | ST LSM6DSV16X (6-axis) | $2-4 vs BNO085 $15-25, embedded SFLP, same feature vector |
| D2 | Magnetometer | Not needed | 6-axis SFLP fusion sufficient for hand posture |
| D3 | IMU per glove | Single IMU | One IMU per glove matches V5 architecture, cost-effective |
| D4 | Fusion library | ST official LSM6DSV16X driver + Madgwick filter | MCU-side ready-made library, SFLP quaternion output |
| D5 | Flex sensor | SpectraFlex 2.2" | Domestic for dev, Spectra Symbol for production |
| D6 | Pull-down resistor | 47kΩ | Optimal voltage divider range |
| D7 | Feature dimensions | 11 per hand | Minimum viable for gesture classification |
| D8 | I2C topology | Flat bus (no MUX) | LSM6DSV16X@0x6A + ADS1115@0x48 + ADS1115@0x49 |
| D9 | Calibration | 6s (3s open + 3s fist) | Sufficient for 5-point piecewise linear |
| D10 | Receiver | P4 smart base station | Inference + display + audio, standalone capable |
| D11 | Tier2 model | Gated Bi-CrossAttn (~80KB) | Fits ESP32-P4, cross-hand attention |
| D12 | Frontend | React3F MVP + Unity XR parallel | Fast validation + XR device support |
| D13 | NLP/CTC | Phased: isolated first, CTC later | Reduces Phase 1 complexity |
| D14 | Migration strategy | New branch + simulation-first | End-to-end validation before hardware |

---

## 2. Hardware Layer

### 2.1 BOM (Per Glove)

| Component | Model | Qty | Interface | Cost | Notes |
|-----------|-------|-----|-----------|------|-------|
| MCU | ESP32-S3-DevKitC-1 N16R8 | 1 | - | ~¥30 | 8MB Flash + 8MB PSRAM |
| IMU | **LSM6DSV16X** | 1 | I2C 0x6A | **~¥3** | 6-axis, SFLP fusion, LGA-14L |
| ADC | ADS1115 (4ch 16-bit) | 2 | I2C 0x48, 0x49 | ~¥8 | 2-3 flex sensors per ADC |
| Flex sensor | SpectraFlex 2.2" | 5 | Analog → ADS1115 | ~¥15 | Domestic alternative for dev |
| Pull-down resistor | 47kΩ | 5 | - | ~¥0.5 | Voltage divider |
| Wiring | Breadboard + Dupont | - | - | ~¥5 | Development phase |
| **Total per glove** | | | | **~¥61.5** | **Was ~¥78 with BNO085** |

**Cost savings**: BNO085 ($15-25) → LSM6DSV16X ($2-4) = **saves $13-21 per IMU, $26-42 per pair**.

### 2.2 Base Station (Unchanged from V5.2)

| Component | Model | Qty | Notes |
|-----------|-------|-----|-------|
| P4 Board | ESP32-P4-Function-EV-Board v1.5.2 | 1 | Competition-provided, 400MHz RV32 |
| C6 Module | ESP32-C6-MINI-1 | 1 | ESP-NOW + BLE 5.0 co-processor |
| MicroSD | 16GB Class10 | 1 | TTS PCM storage |
| USB-C cable | USB 2.0 HS | 1 | P4 to PC connection |

### 2.3 I2C Topology (Flat Bus, No MUX)

```
ESP32-S3 (SDA=GPIO8, SCL=GPIO9, 400kHz)
  ├── LSM6DSV16X  @ 0x6A  (SDO/SA0=GND)    ← NEW: replaces BNO085@0x4B
  ├── ADS1115 #1  @ 0x48  (Ch0=Thumb, Ch1=Index, Ch2=Middle)
  └── ADS1115 #2  @ 0x49  (Ch3=Ring, Ch4=Pinky)
```

**Address change**: BNO085@0x4B → LSM6DSV16X@0x6A (SDO/SA0=GND) or @0x6B (SDO/SA0=VDD)

### 2.4 LSM6DSV16X Wiring (ESP32-S3-DevKitC-1 N16R8)

| LSM6DSV16X Pin | ESP32-S3 Pin | Notes |
|----------------|--------------|-------|
| VDD | 3.3V | **1.71-3.6V, NOT 5V!** |
| VDDIO | 3.3V | I/O supply voltage |
| GND | GND | |
| SDA/SDI | GPIO8 | 4.7kΩ pull-up to 3.3V |
| SCL/SCLK | GPIO9 | 4.7kΩ pull-up to 3.3V |
| SDO/SA0 | GND | LOW=0x6A, HIGH=0x6B |
| CS | 3.3V | HIGH=I2C mode, LOW=SPI mode |
| INT1 | GPIO10 | Optional: data-ready interrupt |
| INT2 | Floating | Not used |

**Critical Notes**:
- CS=HIGH for I2C mode (CS=LOW selects SPI, same as BNO085 PS0 behavior)
- SDO/SA0 pin latched at power-up — do not change during operation
- VDD and VDDIO can be tied together for single 3.3V supply
- 14-pin LGA package, 2.5×3×0.83mm — needs breakout board for breadboard

### 2.5 Flex Sensor Wiring (Unchanged from V5)

```
3.3V ──┤Flex Sensor├── ADC Input ──┤47kΩ├── GND
```

### 2.6 ADS1115 Configuration (Unchanged from V5)

- Gain: ±4.096V (PGA=1), covers 0-3.3V flex sensor range
- Sample rate: 860 SPS per channel; 4 channels polled ≈ 215Hz/channel, sufficient for 100Hz
- Data format: 16-bit signed → normalize to [0, 1]

### 2.7 LSM6DSV16X Key Specifications

| Parameter | Value | Notes |
|-----------|-------|-------|
| Type | 6-axis (accel + gyro) | No magnetometer |
| Accel range | ±16g | Configurable: ±2/4/8/16g |
| Gyro range | ±4000dps | Configurable: ±125/250/500/1000/2000/4000 |
| Accel noise | 70 µg/√Hz | Ultra-low noise |
| Gyro noise | 3.8 mdps/√Hz | Ultra-low noise |
| ODR | Up to 7680Hz | We use 100Hz for power saving |
| Current (high-perf) | 0.65 mA | Accel+Gyro combo |
| Current (low-power) | 0.17 mA | Accel only, 1.6Hz |
| FIFO | 4.5KB | 3-axis/9-axis/6-axis modes |
| SFLP | Embedded | Sensor Fusion Low Power, quaternion output |
| SFLP heading accuracy | 0.5°/5min | Static |
| SFLP pitch/roll accuracy | 1.5° | Static |
| SFLP calibration time | 0.8s | Fast convergence |
| SFLP stabilization | 0.7s | After power-on |
| Supply voltage | 1.71-3.6V | Compatible with 3.3V ESP32 |
| Interface | I2C 400kHz-1MHz / SPI 10MHz / MIPI I3C | I2C used |
| Package | LGA-14L, 2.5×3×0.83mm | Needs breakout board |
| MLC | 16 classes | Machine Learning Core (future use) |
| FSM | Programmable | Finite State Machine (future use) |
| Qvar | Electrode detection | Not used in V6 |

---

## 3. Firmware Layer

### 3.1 FreeRTOS Task Allocation (Per Glove — Unchanged from V5)

| Task | Core | Priority | Freq | Purpose |
|------|------|----------|------|---------|
| Task_SensorRead | Core 1 | 3 (highest) | 100Hz | I2C read ADS1115×2 + LSM6DSV16X, Kalman filter |
| Task_Inference | Core 0 | 2 | ~30Hz | Tier1 CNN inference, output gesture_id + confidence |
| Task_Comms | Core 0 | 1 | 100Hz | ESP-NOW send + BLE provisioning |
| Task_Watchdog | Core 0 | 0 (lowest) | 1Hz | Watchdog feed, health check |

### 3.2 Data Structures (`data_structures.h` — Interface Unchanged)

```cpp
// V6 constants — identical to V5, no Hall-related constants
#define NUM_FLEX_SENSORS     5
#define IMU_FEATURE_COUNT    6     // 3 euler + 3 gyro
#define SINGLE_HAND_FEATURES (NUM_FLEX_SENSORS + IMU_FEATURE_COUNT)  // 11
#define DUAL_HAND_FEATURES   (2 * SINGLE_HAND_FEATURES + 6)          // 28
#define WINDOW_SIZE          30
#define NUM_CLASSES          46
#define SENSOR_RATE_HZ       100

struct SensorData {
    float flex[NUM_FLEX_SENSORS];     // 5 flex sensor normalized values
    float quaternion[4];               // LSM6DSV16X SFLP quaternion (w,x,y,z)
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
    uint8_t  version;                   // 6
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

**Key**: `SensorData` interface is **identical** to V5. All downstream code (inference, relay, frontend) is **zero changes**.

### 3.3 Firmware Modules — V6 Changes

| Module | File | Status | Description |
|--------|------|--------|-------------|
| **LSM6DSV16XManager** | `lib/Sensors/LSM6DSV16XManager.h` | **NEW** | ST driver wrapper, SFLP quaternion + gyro, I2C 0x6A |
| **MadgwickFilter** | `lib/Filters/MadgwickFilter.h` | **NEW** | 6-axis sensor fusion fallback (if SFLP unavailable) |
| ADS1115Manager | `lib/Sensors/ADS1115Manager.h` | Unchanged | Dual ADS1115 driver, I2C 0x48+0x49 |
| FlexManager | `lib/Sensors/FlexManager.h` | Unchanged | Read from ADS1115, 5-point calibration |
| **SensorManager** | `lib/Sensors/SensorManager.h` | **Modified** | Replace BNO085 calls with LSM6DSV16X calls |
| ESP-NOW Comms | `lib/Comms/ESPNOWTransmitter.h` | Unchanged | 69-byte broadcast packet |
| Tier1 Model | `lib/Inference/Tier1Model.h` | Unchanged | Input 11-dim, output 46 classes |
| KalmanFilter1D | `lib/Filters/KalmanFilter1D.h` | Unchanged | 11-channel filter |
| SlidingWindow | `lib/Inference/SlidingWindow.h` | Unchanged | 30-frame ring buffer |
| TFLiteModel | `lib/Models/TFLiteModel.h` | Unchanged | TFLite Micro engine |
| ModelRegistry | `lib/Models/ModelRegistry.h` | Unchanged | Hot-switching |

### 3.4 Removed Modules (V5 → V6)

| Module | Reason |
|--------|--------|
| **BNO085 driver** (Adafruit_BNO08x) | Replaced by LSM6DSV16XManager |
| TMAG5273.h | Hall sensor — removed in V5 |
| TCA9548A.h | I2C MUX — removed in V5 |

### 3.5 LSM6DSV16XManager Design

```cpp
class LSM6DSV16XManager {
public:
    bool begin();                           // I2C init, SFLP enable
    bool readSensor(SensorData& data);      // Read accel+gyro+SFLP quaternion
    bool isReady() const;
    void reset();                           // Software reset

private:
    // I2C register read/write
    uint8_t readRegister(uint8_t reg);
    void writeRegister(uint8_t reg, uint8_t value);
    void readRegisters(uint8_t reg, uint8_t* buf, size_t len);

    // SFLP configuration
    void enableSFLP();                      // Enable embedded sensor fusion
    void setODR(uint16_t hz);              // Set output data rate
    void setAccelRange(uint8_t range);      // ±2/4/8/16g
    void setGyroRange(uint16_t range);      // ±125/250/500/1000/2000/4000dps

    // Quaternion → Euler conversion
    void quaternionToEuler(const float q[4], float euler[3]);

    // State
    bool initialized_;
    uint8_t i2c_addr_;                      // 0x6A (SDO=GND) or 0x6B (SDO=VDD)
};
```

### 3.6 SFLP (Sensor Fusion Low Power) Configuration

The LSM6DSV16X has an embedded SFLP block that outputs quaternions directly — no MCU-side fusion computation needed.

**Register sequence to enable SFLP**:
```
1. CTRL1_XL = 0x50    // Accel ODR=104Hz, ±16g
2. CTRL2_G  = 0x50    // Gyro ODR=104Hz, ±2000dps
3. FUNC_CFG_ACCESS = 0x80  // Enable embedded register bank
4. EMB_FUNC_EN_A = 0x01   // Enable SFLP (bit 0)
5. FUNC_CFG_ACCESS = 0x00  // Lock embedded register bank
6. EMB_FUNC_EN_B = 0x02   // Enable SFLP_GAME_EN (game rotation vector)
```

**Reading quaternion**:
```
Read 8 bytes from EMB_FUNC_OUTPUT registers (0x04-0x0B)
→ 4 × int16 (Q14 format) → normalize → float quaternion[4]
```

### 3.7 MadgwickFilter (Fallback)

If SFLP is unavailable or disabled, use Madgwick filter on MCU:

```cpp
class MadgwickFilter {
public:
    MadgwickFilter(float beta = 0.1f, float sampleFreq = 100.0f);
    void update(float gx, float gy, float gz,
                float ax, float ay, float az);
    void getQuaternion(float q[4]) const;
    void getEuler(float euler[3]) const;

private:
    float q0_, q1_, q2_, q3_;  // Quaternion state
    float beta_;                 // Filter gain
    float sampleFreq_;
};
```

**Performance**: Madgwick filter on ESP32-S3 @ 240MHz ≈ **0.05ms per update** — negligible.

### 3.8 SensorManager Changes (BNO085 → LSM6DSV16X)

```cpp
// V5 (BNO085)
// #include <Adafruit_BNO08x.h>
// Adafruit_BNO08x bno;
// bno.enableReport(SH2_ROTATION_VECTOR);
// bno.enableReport(SH2_GYROSCOPE_CALIBRATED);

// V6 (LSM6DSV16X)
#include "LSM6DSV16XManager.h"
LSM6DSV16XManager imu;

bool SensorManager::begin() {
    // ... ADS1115 init (unchanged) ...
    if (!imu.begin()) {  // I2C 0x6A
        ESP_LOGE(TAG, "LSM6DSV16X init failed");
        return false;
    }
    return true;
}

bool SensorManager::readIMU(SensorData& data) {
    return imu.readSensor(data);  // Populates quaternion, euler, gyro
}
```

**Lines changed**: ~30 lines in SensorManager.cpp (replace BNO085 API calls with LSM6DSV16X calls).

---

## 4. Communication Protocol

### 4.1 Three-Level Communication Chain (Unchanged from V5)

```
Glove → ESP-NOW → C6 → UART 2Mbps → P4 → USB HS → PC Relay → WebSocket → Frontend
       (69B,2ms)       (73B frame)      (Protobuf)     (JSON)
```

### 4.2 ESP-NOW (Glove → C6)

- 69-byte `GlovePacket` struct (defined in Section 3.2)
- Broadcast mode, no pairing
- C6 broadcasts SYNC_TICK every 10ms; gloves use received tick_id for frame alignment
- Sync precision: <2ms (ESP-NOW ~1ms + sampling jitter ~1ms)
- Packet loss handling: linear interpolation for up to 3 consecutive dropped frames

### 4.3 C6 → P4 UART Protocol

| Parameter | Value |
|-----------|-------|
| Baud rate | 2 Mbps |
| TX pin | GPIO43 (C6) → GPIO38 (P4) |
| RX pin | GPIO44 (C6) ← GPIO37 (P4) |
| Format | 8N1 |
| Frame | `[0xAA][0x55][LEN_H][LEN_L][PAYLOAD...][CRC16_H][CRC16_L]` |

### 4.4 P4 Aggregation Logic

1. Receive left/right packets via UART, pair by tick_id
2. Compute 6-dim relative features:
   - `ΔEuler[3]`: euler_L[i] - euler_R[i] for i in (roll, pitch, yaw)
   - `ΔQuatDist[1]`: 2 * arccos(|q_L · q_R|) — quaternion angular distance
   - `ΔGyroNorm[1]`: ||gyro_L|| - ||gyro_R|| — angular speed difference
   - `ΔGyroAxis[1]`: argmax(|gyro_L - gyro_R|) / 3 — dominant axis (normalized)
3. Output 28-dim feature vector
4. Forward via USB HS (Protobuf encoded) to PC

### 4.5 Protobuf Schema (v6)

```protobuf
syntax = "proto3";
package echoglove_v6;

message GloveData {
    uint32 version = 1;         // 6
    uint32 timestamp = 2;
    uint32 hand_id = 3;         // 0=left, 1=right
    repeated float flex = 4;    // [5]
    repeated float imu = 5;     // [6]
    uint32 l1_gesture_id = 6;
    float l1_confidence = 7;
    repeated float relative = 8; // [6]
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

### 4.6 WebSocket (PC → Frontend)

- JSON format, port 8765
- Fields: `tier`, `blend_alpha`, `left_hand`, `right_hand`, `relative`, `inference`, `nlp_text`

---

## 5. Relay Layer

### 5.1 Three-Tier Architecture

| Tier | Location | Model | Input | Output | Size | Latency |
|------|----------|-------|-------|--------|------|---------|
| Tier1 | Glove ESP32-S3 | CNN+SE-Attention | 11-dim single hand | 46-class probs | ~80KB INT8 | <10ms |
| Tier2 | P4 Base Station | Gated Bi-CrossAttn | 28-dim dual hand | 46-class probs | ~80KB INT8 | ~30ms |
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
- Edges: 40 intra-hand + 6 cross-hand + 42 self-loops = **88 total**
- Tier1/2: 12-node simplified graph (Wrist + 5 fingertips × 2 hands), 22 edges

### 5.4 Confidence Router (Three-Tier Fusion)

```python
# Always run Tier1
tier1_result = run_tier1(features_11d)

# Run Tier2 when P4 is online
if p4_online:
    tier2_result = run_tier2(features_28d)

# Run Tier3 when PC is online
if pc_online:
    tier3_l1 = run_tier3_l1(window_28d_30frames)
    tier3_l2 = run_tier3_l2(window_28d_30frames)  # CTC decode

# Hot-swap: 5-frame linear blend
output = (1-blend_alpha) * old_tier_result + blend_alpha * new_tier_result
```

### 5.5 NLP Pipeline (Phase 1: Isolated Gestures)

- Grammar corrector: gesture ID sequence → Chinese/English text
- TTS speech synthesis: edge-tts or pre-recorded PCM
- Phase 2: CTC beam search for continuous sign language decoding

### 5.6 Continuous Sign Language Decoding (Phase 2)

```
Raw frames (100Hz)
  → Sliding window (30 frames, 50% overlap)
  → ST-GCN spatial-temporal feature extraction
  → MS-TCN multi-stage temporal segmentation
  → CTC beam search → character/word sequence
  → NLP grammar correction → fluent sentence
  → TTS audio output
```

### 5.7 Model Hot-Switch Architecture

- **BaseModel Interface**: Python (`glove_relay/src/models/base_model.py`) + C++ (`glove_firmware/lib/Models/BaseModel.h`)
- **Model Registry**: Dynamically load/switch models at runtime
- **YAML Config**: `glove_relay/configs/model_config.yaml` defines active model
- **P4 Hot-Switch**: P4 can switch between Tier2 local inference and forwarding to PC Tier3

---

## 6. Frontend

### 6.1 Dual Frontend Strategy

| Frontend | Purpose | Skeleton | Communication |
|----------|---------|----------|---------------|
| React3F (Web) | MVP validation, debug tools | 21-keypoint MediaPipe topology | WebSocket JSON |
| Unity XR Hands | Final product, XR device support | 26-joint OpenXR standard | WebSocket/UDP JSON |

### 6.2 React3F MVP

- Existing 21-keypoint 3D hand scaffold
- Dual-hand rendering (left/right independent objects)
- Debug overlay: raw sensor values, tier selection, confidence
- Vite + React 18 + R3F, path alias `@/` → `src/`

### 6.3 Unity XR Hands Integration

- Unity 2022.3 LTS + XR Hands 1.7 + OpenXR standard
- 26-joint skeleton (per OpenXR spec):
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
// Thumb: CMC dual-DOF (flex + gyro-assisted abduction)
thumb.CMC_flexion  = flex[0] * 1.0f;
thumb.CMC_abduction = imu_gyro_y * 0.5f;  // LSM6DSV16X gyro assist
thumb.MCP = flex[0] * 0.8f;
thumb.IP  = flex[0] * 0.6f;

// Other four fingers: linear coupling
for (i = 1..4) {
    finger[i].MCP = flex[i] * 1.0f;
    finger[i].PIP = flex[i] * 0.67f;
    finger[i].DIP = flex[i] * 0.5f;
    finger[i].TIP = flex[i] * 0.3f;  // Passive, follows
}

// Wrist: LSM6DSV16X SFLP quaternion
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
4. LSM6DSV16X auto-calibration: SFLP converges in 0.8s (no manual IMU calibration needed)

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

### 7.6 Quantization Strategy

| Tier | Framework | Quantization | Size | Accuracy Loss |
|------|-----------|-------------|------|---------------|
| Tier1 | TFLite Micro | INT8 (full integer) | ~80KB | <2% vs FP32 |
| Tier2 | TFLite Micro | INT8 (full integer) | ~80KB | <2% vs FP32 |
| Tier3-L1 | PyTorch | FP32 | ~500KB | N/A |
| Tier3-L2 | PyTorch | FP32 | ~3MB | N/A |

**INT8 calibration**: Representative dataset of 100 samples from training set.

### 7.7 Flex Sensor Temperature Drift Compensation

Flex sensors exhibit resistance drift with temperature (~0.5%/°C).

**Compensation strategy**:
1. Record ambient temperature at calibration time (LSM6DSV16X has built-in temperature sensor)
2. During runtime, read temperature periodically (every 10s)
3. Apply linear correction: `flex_corrected = flex_raw * (1 + 0.005 * (T_cal - T_current))`
4. If temperature drift >5°C from calibration, prompt user to recalibrate

### 7.8 ESP-NOW Packet Loss Handling

| Scenario | Strategy | Max Gap |
|----------|----------|---------|
| 1-3 dropped frames | Linear interpolation | 30ms |
| 4-10 dropped frames | Hold last valid value + flag | 100ms |
| >10 dropped frames | Mark hand as offline, Tier1 fallback | >100ms |

**Implementation**: Ring buffer of last 4 valid packets. On gap, interpolate between last_valid and next_valid.

---

## 8. Migration Execution Plan

### Phase 0: Branch & Setup (1 day)

- Create `V6-LSM6DSV16X` branch from `V5-DualGloveFlex`
- Update CLAUDE.md, PROGRESS.md with V6 context
- Order LSM6DSV16X breakout boards (if not already available)
- Verify: branch builds clean with `pio run`

### Phase 1: LSM6DSV16X Driver (3 days)

- Create `lib/Sensors/LSM6DSV16XManager.h/.cpp`
- Implement I2C register read/write (0x6A)
- Implement SFLP enable sequence
- Implement quaternion + gyro reading
- Implement `MadgwickFilter` as fallback
- Verify: I2C scan detects LSM6DSV16X at 0x6A, quaternion output stable

### Phase 2: SensorManager Integration (2 days)

- Modify `SensorManager.h/.cpp` to use LSM6DSV16XManager instead of BNO085
- Verify `SensorData` struct populated correctly (quaternion, euler, gyro)
- Verify Kalman filter works with new IMU data
- Remove Adafruit_BNO08x dependency from `platformio.ini`
- Verify: `pio run` clean build, single glove sensor readings via serial monitor

### Phase 3: End-to-End Validation (3 days)

- Single glove → ESP-NOW → C6 → P4 → USB → PC relay
- Verify 11-dim feature vector matches V5 format
- Verify 28-dim dual-hand features computed correctly
- Run existing relay test suite (should pass with zero changes)
- Verify: full data path works, frontend renders hand correctly

### Phase 4: Model Retraining (5 days)

- Collect new dataset with LSM6DSV16X (if IMU characteristics differ from BNO085)
- Retrain Tier1 (CNN+SE-Attention) if needed
- Retrain Tier2 (Gated Bi-CrossAttn) if needed
- Export TFLite INT8 models
- Verify: model accuracy Tier1 >85%, Tier2 >90%

### Phase 5: Integration & Polish (3 days)

- Full chain: glove → C6 → P4 → PC → frontend
- Hot-swap testing (Tier1 ↔ Tier2 ↔ Tier3)
- Latency testing (E2E < 100ms target)
- Stability testing (30-minute continuous run)
- Verify: competition demo ready

**Total: ~17 days (approximately 3 weeks)**

---

## 9. Reference Repositories & Papers

### Top 5 Direct References

1. **Unity XR Hands**: Dual-hand virtual display, XR interaction
2. **Smart-Sign-Language-Translator-Glove** (Gill003): Sensor calibration, ESP32 communication
3. **CASA0018-Gloves-Edge-AI** (ReikiC): ESP32 lightweight model deployment
4. **Nature 2024** (Stretchable glove): Flex sensor + IMU fusion, pose reconstruction
5. **PenSLR** (arXiv 2406.16388): End-to-end sign language recognition with CTC

### LSM6DSV16X Specific References

1. **ST LSM6DSV16X Datasheet** (Rev 4, May 2023): Register map, SFLP configuration
2. **ST AN5763**: LSM6DSV16X application note, SFLP tuning guide
3. **ST github.com/STMicroelectronics/STMems_Standard_C_drivers**: Official C driver
4. **Madgwick filter**: Original paper "An efficient orientation filter for inertial and inertial/magnetic sensor arrays" (2010)

---

## 10. Risk Register

| Risk | Probability | Impact | Mitigation |
|------|-------------|--------|------------|
| LSM6DSV16X SFLP accuracy vs BNO085 | Medium | Medium | SFLP heading 0.5°/5min is comparable; Madgwick fallback available |
| LSM6DSV16X breakout board availability | Low | High | Order early; LGA-14L needs custom PCB or commercial breakout |
| I2C address conflict (0x6A vs 0x48/0x49) | None | None | No conflict — different addresses |
| Flex sensor non-linearity | Medium | Medium | 5-point piecewise calibration, per-user calibration |
| ESP-NOW packet loss | Medium | Medium | Linear interpolation for up to 3 dropped frames |
| Training data insufficient | High | High | Start with 46 classes, expand incrementally |
| Model accuracy regression after IMU change | Medium | High | Retrain with new IMU data; feature vector unchanged |
| LSM6DSV16X power-on initialization timing | Low | Medium | SFLP stabilization 0.7s; add delay in firmware init |
| Temperature drift of flex sensors | Medium | Low | LSM6DSV16X built-in temp sensor for compensation |

---

## 11. Open Questions

None remaining — all design decisions confirmed in brainstorming sessions (2026-06-23).

---

## 12. Innovation Points

| # | Innovation | Level | Description |
|---|-----------|-------|-------------|
| 1 | LSM6DSV16X SFLP embedded fusion | Hardware | 0.65mA, 0.8s calibration, quaternion output — no MCU compute |
| 2 | Three-tier inference hot-switch | System | Glove→P4→PC auto-downgrade, standalone capable |
| 3 | P4 smart base station | System | Inference + display + audio, no PC needed |
| 4 | Gated Bi-CrossAttention | Academic | Dual-hand cross-attention for sign language |
| 5 | Cost-optimized IMU migration | Engineering | $26-42 savings per pair, same feature vector |
| 6 | 28-dim dual-hand features | Engineering | Relative pose features for cross-hand gestures |
| 7 | Temperature drift compensation | Engineering | LSM6DSV16X temp sensor + flex sensor correction |
| 8 | Standalone sign language translator | System | Complete hand-sign→text+voice without PC |
