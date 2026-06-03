# EchoGlove-SLR-MOCAP V4 SOP-SPEC-PLAN
## Edge-AI Data Glove Migration: IMU+Flex Architecture

**Version:** V4.0-MIGRATION  
**Date:** 2026-05-31  
**Author:** Paxon Huang / LapinEX Studio  
**Status:** Migration Specification (Hall→Flex)  
**Previous:** V3 (IMU+Hall+Magnet) — Deprecated due to magnetic crosstalk

---

## 1. Executive Summary

### 1.1 Migration Rationale
V3 architecture (BNO085 + 5× TMAG5273 + 5× fingertip magnets) suffers from **fatal magnetic crosstalk**:
- Fingertip NdFeB magnets generate >100mT at 1cm, 0.5–5mT at 5cm
- TMAG5273 range (±40mT) is overwhelmed by multi-magnet superposition
- BNO085 magnetometer suffers hard/soft iron interference, causing Yaw drift
- Multi-magnet nonlinear coupling makes inverse modeling ill-conditioned

**Decision:** Migrate to **IMU (BNO085, 6DOF Game Rotation Vector) + 5× Flex Sensors (Spectra Symbol 2.2" or domestic equivalent)**. This is the most academically validated, cost-effective, and manufacturable architecture for sign-language data gloves.

### 1.2 Target Metrics (Preserved from V3)
| Metric | Target | Notes |
|--------|--------|-------|
| L1 inference latency | <3ms | Edge ESP32-S3 |
| L2 inference latency | <20ms | Python Relay |
| End-to-end latency | <100ms | Full pipeline |
| L1 accuracy (46 classes) | >90% Top-1 | Requires dataset recollection |
| L2 accuracy (46 classes) | >95% Top-1 | ST-GCN retrain |
| Sensor sampling rate | 100Hz | Unchanged |
| BOM Cost | ~$55 | +$16 from V3 ($38→$54) |

---

## 2. Hardware Architecture V4

### 2.1 System Block Diagram

```
┌─────────────────────────────────────────────────────────────┐
│                    ECHOGLOVE V4 HARDWARE                     │
│                                                              │
│  ┌─────────────┐    ┌─────────────┐    ┌─────────────┐     │
│  │  Flex ×5    │    │  ADS1115    │    │  ADS1115    │     │
│  │  (2.2")     │───→│  #1 (4ch)   │    │  #2 (1ch)   │     │
│  │  Voltage    │    │  I2C 0x48   │←──→│  I2C 0x49   │     │
│  │  Divider    │    │  16-bit ADC │    │  16-bit ADC │     │
│  └─────────────┘    └──────┬──────┘    └──────┬──────┘     │
│                            │                    │           │
│  ┌─────────────┐    ┌──────┴────────────────────┴──────┐   │
│  │  BNO085     │    │         ESP32-S3-DevKitC-1         │   │
│  │  (6DOF)     │←──→│  Core1: 100Hz ADC+I2C Sampling   │   │
│  │  I2C 0x4A   │    │  Core0: L1 Inference + UDP Comm  │   │
│  └─────────────┘    │  8MB Flash + 8MB PSRAM             │   │
│                     │  GPIO8(SDA) / GPIO9(SCL)           │   │
│                     └────────────────────────────────────┘   │
│                                                              │
│  Power: 3.3V LDO (from LiPo 3.7V via TP4056+AMS1117)       │
│  Battery: 603450 LiPo 1200mAh                                │
└─────────────────────────────────────────────────────────────┘
```

### 2.2 Sensor Specifications

#### 2.2.1 Flex Sensor — Spectra Symbol 2.2" (or Domestic Equivalent)
| Parameter | Value |
|-----------|-------|
| Type | Resistive flex sensor (conductive ink on PET) |
| Length | 2.2" (55.88mm) |
| Flat resistance | ~10kΩ–35kΩ (varies by batch; calibrate per unit) |
| Bent resistance (90°) | ~60kΩ–100kΩ |
| Resistance change | Increases with bend |
| Durability | >1M bend cycles (avoid sharp creases) |
| Operating temp | -35°C to +80°C |
| Cost | $8–12/unit (Spectra Symbol) / $3–5/unit (domestic) |

**Voltage Divider Design:**
```
3.3V ──[ Flex Sensor ]──┬──[ R_fixed ]── GND
                        │
                      ADC_IN
```
- **R_fixed = 22kΩ** (1% precision metal film)
- Flat output: ~1.3V (typical)
- Bent output: ~0.7V (typical)
- Dynamic range: ~0.6V, centered in ADS1115 PGA optimal range
- Add **100nF ceramic cap** from ADC_IN to GND for hardware low-pass filtering

**Domestic Alternative:** 国产弯曲传感器 (DFRobot/嘉立创/淘宝定制导电油墨型), 2.2" 规格, 阻值范围相近, 需批次标定。

#### 2.2.2 BNO085 — 6DOF Game Rotation Vector Mode
| Parameter | Value |
|-----------|-------|
| Mode | **SH2_GAME_ROTATION_VECTOR** (6DOF, Accel+Gyro only) |
| Output | Quaternion (w, x, y, z) |
| Update rate | 100Hz |
| I2C Address | 0x4A (default) |
| Why 6DOF? | Magnetometer disabled to eliminate magnetic interference from flex sensors (none) and ambient sources |
| Yaw drift | ~5–10°/min (acceptable for sign language; gestures are relative, not absolute geo-heading) |

> **Critical:** Do NOT use SH2_ROTATION_VECTOR (9DOF) or SH2_GEOMAGNETIC_ROTATION_VECTOR. These use magnetometer and will be unstable even without magnets (soft iron from battery, PCB traces).

#### 2.2.3 ADS1115 — 16-bit ADC with I2C
| Parameter | Value |
|-----------|-------|
| Resolution | 16-bit (signed, ±32768) |
| Channels | 4 single-ended per chip |
| PGA | Programmable: ±6.144V to ±0.256V |
| Data Rate | 8SPS to 860SPS |
| I2C Address | 0x48 (ADDR→GND), 0x49 (ADDR→VDD) |
| Supply | 2.0V–5.5V (3.3V compatible) |
| Current | 150μA continuous conversion |
| Cost | ~$2–3/module (aliexpress) |

**Why ADS1115 over ESP32 internal ADC?**
1. ESP32-S3 internal ADC has documented non-linearity, especially near rails (0V, 3.3V) and in mid-range
2. ADC2 is unavailable when WiFi/BT active (critical for UDP communication)
3. ADS1115 provides stable 16-bit resolution, independent reference, PGA amplification
4. I2C interface aligns with existing TCA9548A removal (no new protocol stack)
5. Two ADS1115 modules cost <$6, negligible vs. system cost

**Why not CD74HC4067?**
- CD74HC4067 has ~70Ω ON resistance, causing voltage drop and thermal drift
- Requires ESP32 internal ADC, reintroducing accuracy problems
- Adds GPIO pin demand (4 select lines + 1 ADC pin)
- ADS1115 is digital, noise-immune, and requires only 2 wires (I2C)

### 2.3 Pin Assignment (ESP32-S3)

| Pin | Function | Direction | Notes |
|-----|----------|-----------|-------|
| GPIO8 | I2C_SDA | Bidir | 4.7kΩ pull-up to 3.3V |
| GPIO9 | I2C_SCL | Output | 4.7kΩ pull-up to 3.3V |
| GPIO34 | ADC_EXT (reserved) | Input | Reserved for future expansion |
| GPIO0 | Boot/Strapping | — | Do NOT connect voltage divider here |
| 3.3V | Power Rail | — | Max 500mA from AMS1117 |
| GND | Ground | — | Common ground plane |

**I2C Bus Devices:**
| Device | Address | Role |
|--------|---------|------|
| ADS1115 #1 | 0x48 | Flex[0..3] (Index, Middle, Ring, Pinky) |
| ADS1115 #2 | 0x49 | Flex[4] (Thumb) + 3 spare channels |
| BNO085 | 0x4A | IMU Quaternion (6DOF) |

### 2.4 BOM (Bill of Materials) V4

| Item | Qty | Unit Price | Total | Supplier |
|------|-----|------------|-------|----------|
| ESP32-S3-DevKitC-1 N16R8 | 1 | $8.00 | $8.00 | Espressif |
| BNO085 (Cetus/Adafruit) | 1 | $12.00 | $12.00 | Adafruit/DFRobot |
| ADS1115 Module | 2 | $2.50 | $5.00 | AliExpress |
| Flex Sensor 2.2" (Spectra) | 5 | $10.00 | $50.00 | SparkFun/Adafruit |
| **OR** Domestic Flex 2.2" | 5 | $4.00 | $20.00 | Taobao/JLC |
| 22kΩ 1% Resistor | 5 | $0.02 | $0.10 | JLCPCB |
| 100nF Ceramic Cap | 5 | $0.02 | $0.10 | JLCPCB |
| 4.7kΩ Pull-up Resistor | 2 | $0.02 | $0.04 | JLCPCB |
| LiPo 603450 1200mAh | 1 | $3.00 | $3.00 | Taobao |
| TP4056 + AMS1117 Module | 1 | $1.50 | $1.50 | Taobao |
| Glove Base (elastic) | 1 | $2.00 | $2.00 | Custom |
| PCB/Protoboard | 1 | $3.00 | $3.00 | JLCPCB |
| **TOTAL (Spectra)** | | | **$84.74** | |
| **TOTAL (Domestic)** | | | **$54.74** | |

> Note: BOM increased from V3 ($38) primarily due to flex sensors. If using domestic flex sensors, total is ~$55, maintaining affordability.

---

## 3. Firmware Architecture V4

### 3.1 FreeRTOS Task Model (Unchanged from V3 structure)

```
Core 1 (High Priority, 100Hz):  Sensor Sampling Task
  ├── Read ADS1115 #1 (4 flex channels)  ~2ms
  ├── Read ADS1115 #2 (1 flex channel)   ~1ms
  ├── Read BNO085 (6DOF Quaternion)      ~1ms
  ├── Apply EMA filter to flex raw values
  └── Write to RingBuffer

Core 0 (Normal Priority):        Inference + Communication Task
  ├── L1 Model Inference (1D-CNN+Attention)  <3ms
  ├── Protobuf Serialization
  └── UDP Transmission (100Hz)
```

### 3.2 Sensor Data Pipeline

```
Raw ADC (uint16) → Voltage (mV) → Normalized Bend % → EMA Filter → RingBuffer
     ↑                              ↑
   ADS1115                      Calibration
   (16-bit)                     (min/max per sensor)
```

**Calibration Procedure (Per User, Per Session):**
1. **Open Hand:** User holds hand fully open for 3 seconds → record `raw_min`
2. **Fist:** User makes tight fist for 3 seconds → record `raw_max`
3. **Normalize:** `bend_pct = (raw - raw_min) / (raw_max - raw_min) * 100`
4. **Clamp:** `bend_pct = clamp(bend_pct, 0, 100)`
5. **Deadband:** Ignore changes < 1% to reduce jitter

### 3.3 Filter Design

| Sensor | Filter | Parameters | Rationale |
|--------|--------|------------|-----------|
| Flex (raw) | EMA (Exponential Moving Average) | α = 0.15 | Smooth analog noise without lag |
| Flex (pct) | Deadband | ±1% | Eliminate micro-jitter |
| BNO085 Quat | Low-pass (optional) | α = 0.05 | Smooth orientation jitter |

**Why not Kalman?**
- V3 used Kalman for magnetometer fusion. With 6DOF BNO085, internal SH-2 already handles sensor fusion.
- Flex sensors are scalar, 1D; EMA is sufficient and computationally cheaper than Kalman on ESP32-S3.

### 3.4 Protobuf Schema V4

```protobuf
syntax = "proto3";

message SensorFrame {
  uint32 timestamp_ms = 1;      // ESP32 millis()

  // Flex sensors: 5 channels, 0-100% bend (uint8 sufficient)
  uint32 flex_thumb    = 2;     // 0-100 (ADS1115 #2, ch0)
  uint32 flex_index    = 3;     // 0-100 (ADS1115 #1, ch0)
  uint32 flex_middle   = 4;     // 0-100 (ADS1115 #1, ch1)
  uint32 flex_ring     = 5;     // 0-100 (ADS1115 #1, ch2)
  uint32 flex_pinky    = 6;     // 0-100 (ADS1115 #1, ch3)

  // BNO085 6DOF Quaternion (int16 fixed-point, Q15 format)
  sint32 quat_w = 7;            // /32768.0 to get float
  sint32 quat_x = 8;
  sint32 quat_y = 9;
  sint32 quat_z = 10;

  // Metadata
  uint32 seq_num = 11;          // Frame sequence for packet loss detection
  uint32 battery_mv = 12;       // LiPo voltage (optional)
}
```

**Payload Size:** ~32 bytes/frame → 3.2 KB/s at 100Hz (vs V3 ~240 bytes/s, actually smaller due to simpler data)

---

## 4. Communication Protocol V4

### 4.1 UDP Layer (Unchanged)
- **Port:** 8888
- **Rate:** 100Hz
- **Format:** Nanopb Protobuf (binary)
- **MTU:** 512 bytes (well within Ethernet/ WiFi limits)

### 4.2 WebSocket Layer (Unchanged)
- **Port:** 8765
- **Format:** JSON
- **Rate:** 100Hz (throttled from UDP)

### 4.3 Schema Change Impact
| Field | V3 | V4 | Frontend Impact |
|-------|----|----|-----------------|
| mag[5][3] | 15 floats | — | Removed |
| flex[5] | — | 5 uint8 | Added; R3F/Unity reads bend% directly |
| quat[4] | 4 floats | 4 int16 (Q15) | Precision reduced but sufficient; divide by 32768 |

---

## 5. Machine Learning Architecture V4

### 5.1 Input Feature Vector

| Feature | Dim | Source | Range |
|---------|-----|--------|-------|
| flex_thumb | 1 | ADS1115 #2 | 0–100 |
| flex_index | 1 | ADS1115 #1 | 0–100 |
| flex_middle | 1 | ADS1115 #1 | 0–100 |
| flex_ring | 1 | ADS1115 #1 | 0–100 |
| flex_pinky | 1 | ADS1115 #1 | 0–100 |
| quat_w | 1 | BNO085 | -1.0–1.0 |
| quat_x | 1 | BNO085 | -1.0–1.0 |
| quat_y | 1 | BNO085 | -1.0–1.0 |
| quat_z | 1 | BNO085 | -1.0–1.0 |
| **Total** | **9** | | |

### 5.2 L1 Model: 1D-CNN + Attention (Modified)

**Architecture Changes from V3:**
- Input channels: 19 → **9**
- Conv1D kernel: 3 (unchanged)
- Conv1D channels: 64 → **32** (reduce overfitting with smaller input)
- Attention heads: 4 → **2**
- Attention dim: 64 → **32**
- FCN hidden: 128 → **64**

**Rationale:** Input dimension halved; model capacity should reduce proportionally to prevent overfitting on smaller feature space.

```python
# Pseudocode
class L1Model(nn.Module):
    def __init__(self, num_classes=46):
        super().__init__()
        self.conv = nn.Conv1d(9, 32, kernel_size=3, padding=1)  # 9 channels
        self.attn = nn.MultiheadAttention(embed_dim=32, num_heads=2, batch_first=True)
        self.fc = nn.Sequential(
            nn.Linear(32 * seq_len, 64),
            nn.ReLU(),
            nn.Dropout(0.3),
            nn.Linear(64, num_classes)
        )
```

### 5.3 L2 Model: ST-GCN (Modified)

**Hand Skeleton Graph:**
- Nodes: 6 (wrist/palm + 5 fingertips)
- Edges: kinematic chain (palm→thumb, palm→index, ..., index→middle adjacency)
- Node features: 9-dim (5 flex + 4 quat) → all nodes share palm quat; finger nodes use individual flex

**Changes:**
- Node feature dim: 19 → **9**
- Graph structure unchanged (topology is physical, not sensor-dependent)
- Retrain required; old weights incompatible

### 5.4 Dataset Requirements

**Critical:** V3 dataset is **NOT reusable**. Must recollect.

**Collection Protocol:**
1. **Subjects:** Minimum 10 signers (5 male, 5 female), varying hand sizes
2. **Classes:** 46 CSL (Chinese Sign Language) gestures (preserved from V3)
3. **Repetitions:** 20 reps per class per subject = 9,200 samples minimum
4. **Duration:** ~2 seconds per gesture
5. **Calibration:** Per-session open-hand/fist calibration before recording
6. **Environment:** Indoor, varying lighting (no magnetic requirements anymore)
7. **Split:** 70% train / 15% val / 15% test (subject-independent split recommended)

---

## 6. Frontend & Rendering V4

### 6.1 R3F (React Three Fiber) — Web MVP

**Changes:**
- Finger bend angle source: `mag_inverse_kinematics()` → `flex_pct * max_angle`
- Max bend angle per finger: Thumb 70°, Index 90°, Middle 90°, Ring 90°, Pinky 80°
- Palm orientation: Quaternion from BNO085 directly (no magnetometer drift compensation needed)
- IK solver: Simplified; flex% maps linearly to joint angle (first-order approximation sufficient for MVP)

```typescript
// Finger bend mapping
const fingerMaxAngles = {
  thumb: 70, index: 90, middle: 90, ring: 90, pinky: 80
};

const bendAngle = (flexPct: number, finger: string) => {
  return (flexPct / 100) * fingerMaxAngles[finger] * (Math.PI / 180);
};
```

### 6.2 Unity Pro — ms-MANO

**Changes:**
- Update `HandPoseDriver` to read `flex[]` array from WebSocket JSON
- Retarget flex% to MANO joint angles using linear mapping + per-finger offset
- Yaw drift from 6DOF: compensate by resetting palm forward vector at gesture start (relative gesture frame)

---

## 7. Migration Roadmap (10 Weeks)

### Phase 1: Hardware Prototyping (Week 1–2)
**Goal:** Validate flex sensor + ADS1115 + BNO085 6DOF on breadboard

| Day | Task | Deliverable |
|-----|------|-------------|
| 1–2 | Purchase 5× flex, 2× ADS1115, 22kΩ resistors, 100nF caps | Components received |
| 3–4 | Breadboard wiring: 5 voltage dividers → ADS1115 → ESP32 | Schematic verified |
| 5–6 | BNO085 I2C verification; switch to Game Rotation Vector | Stable quaternion output |
| 7–8 | 100Hz sampling test; measure latency and jitter | <5ms sampling jitter |
| 9–10 | Calibration procedure test (open/fist) | Repeatable 0–100% mapping |
| 11–12 | Wearability test: sew flex sensors into glove prototype | Comfort assessment |
| 13–14 | **Gate Review:** If accuracy < 80% or comfort fail, pivot to all-IMU | Go/No-Go decision |

### Phase 2: Firmware Migration (Week 3–4)
**Goal:** Rewrite `glove_firmware` for V4 architecture

| Task | Details |
|------|---------|
| Remove TMAG5273 driver | Delete `tmag5273.h/c`, `tmag5273.c` |
| Remove TCA9548A driver | Delete `tca9548a.h/c`; I2C bus now direct |
| Add ADS1115 driver | ESP-IDF compatible; support 0x48 and 0x49; continuous conversion mode; 860SPS |
| Add FlexSensor module | Voltage divider interface; calibration storage in NVS |
| Update BNO085 driver | Force `SH2_GAME_ROTATION_VECTOR` at init; disable mag reports |
| Update Protobuf schema | `sensor_frame.proto` V4; regenerate with nanopb |
| Update filter stack | Replace Kalman with EMA + deadband |
| Update FreeRTOS tasks | Core1: ADS1115 polling loop; Core0: unchanged |
| Power management | Verify current draw < 200mA at 3.3V |

### Phase 3: Communication & Protocol (Week 5)
**Goal:** End-to-end data flow from glove to Relay

| Task | Details |
|------|---------|
| Update Protobuf parser | `glove_relay` parse new `SensorFrame` |
| Update JSON schema | WebSocket payload includes `flex[]` instead of `mag[]` |
| UDP stress test | 100Hz × 1 hour, packet loss < 0.1% |
| Latency benchmark | End-to-end < 100ms confirmed |

### Phase 4: Dataset Recollection & Model Retraining (Week 6–8)
**Goal:** New dataset + trained models

| Task | Details |
|------|---------|
| Data collection app | Web UI for labeling; 10 subjects × 46 classes × 20 reps |
| Calibration enforcement | Open-hand + fist calibration before each session |
| Data augmentation | Add Gaussian noise (σ=2%) to flex; quaternion rotation augmentation |
| L1 training | 1D-CNN+Attention; 200 epochs; early stopping; FLOPs < 50M |
| L2 training | ST-GCN; 150 epochs; batch size 32 |
| Benchmark | Top-1/Top-5 + latency + FLOPs; model pool YAML update |

### Phase 5: Frontend Adaptation (Week 9)
**Goal:** R3F + Unity render correctly with flex data

| Task | Details |
|------|---------|
| R3F skeleton update | Map flex% to finger rotation; test 46 gestures visually |
| Unity MANO update | Retarget flex to MANO joints; drift compensation |
| NLP pipeline | Unchanged (gesture→text mapping preserved) |
| TTS | Unchanged |

### Phase 6: Integration & System Test (Week 10)
**Goal:** Full system validation

| Test | Criteria |
|------|----------|
| Functional | 46 gestures recognized, 3D hand matches real hand |
| Performance | L1 < 3ms, L2 < 20ms, E2E < 100ms |
| Robustness | 30min continuous use, no drift > 15° |
| Usability | Don/doff < 10s, calibration < 5s |
| Battery | > 2 hours continuous operation |

---

## 8. Risk Management

| Risk | Probability | Impact | Mitigation |
|------|-------------|--------|------------|
| Flex sensor lifespan < 6 months | Medium | High | Buy 20× spare; design sensor as replaceable module |
| BNO085 Yaw drift unacceptable | Medium | High | Add gesture-relative frame reset; if fail, upgrade to BHI360 |
| Dataset recollection delays | High | Medium | Start collection immediately in Phase 1; parallel track |
| Domestic flex sensor quality variance | Medium | Medium | Batch calibration; reject outliers > 20% resistance deviation |
| Model accuracy < 90% with 9-dim input | Low | High | If fail, add 2× IMU on index/middle fingers (upgrade path) |
| ADS1115 I2C address conflict | Low | High | Hardwire ADDR pins; verify with I2C scanner at boot |

---

## 9. Appendix

### 9.1 ADS1115 Register Configuration (ESP-IDF)
```c
// Config register for single-ended, continuous conversion, 860SPS
#define ADS1115_CONFIG_CH0 0xC3E3  // AIN0 vs GND, PGA=±4.096V, 860SPS
#define ADS1115_CONFIG_CH1 0xD3E3  // AIN1 vs GND
#define ADS1115_CONFIG_CH2 0xE3E3  // AIN2 vs GND
#define ADS1115_CONFIG_CH3 0xF3E3  // AIN3 vs GND
```

### 9.2 BNO085 Init Sequence (6DOF)
```c
// Disable all reports first
sh2_setSensorEnable(SH2_MAGNETIC_FIELD, false);
sh2_setSensorEnable(SH2_ROTATION_VECTOR, false);  // 9DOF
// Enable 6DOF Game Rotation Vector at 100Hz
sh2_setSensorEnable(SH2_GAME_ROTATION_VECTOR, true);
sh2_setReportInterval(SH2_GAME_ROTATION_VECTOR, 10000);  // 10ms = 100Hz
```

### 9.3 Flex Sensor Calibration NVS Schema
```c
// Stored per-device, per-user (optional)
struct FlexCalibration {
    uint16_t raw_min[5];   // Open hand
    uint16_t raw_max[5];   // Fist
    uint32_t crc32;
};
```

### 9.4 Version History
| Version | Date | Description |
|---------|------|-------------|
| V1 | 2024.04 | Initial concept |
| V2 | 2024.10 | Hall+IMU prototype |
| V3 | 2025.04 | Full stack with TMAG5273 + BNO085 9DOF |
| **V4** | **2026.05** | **Migration to Flex + BNO085 6DOF (this doc)** |

---

*Document generated for EchoGlove V4 Migration. All specifications subject to hardware validation in Phase 1.*
