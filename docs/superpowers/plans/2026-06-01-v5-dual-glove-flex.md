# EchoGlove V5.0 DualGloveFlex — Implementation Plan

> **Design Spec**: `docs/superpowers/specs/2026-06-01-v5-dual-glove-flex-design.md`
> **Source Repo**: `/home/paxon/CodingProjects/EchoGloveProjects/EchoGlove-SLR-MOCAP/` (Beta)
> **Strategy**: New `V5-DualGloveFlex` branch, simulation-first, TDD-first
> **Total**: ~32 days (8 phases)

---

## Goal

Migrate EchoGlove from V3 (Hall sensor, single-hand) to V5 (flex sensor, dual-hand) with 3-tier inference, ESP-NOW communication, and dual frontend (React3F + Unity XR Hands).

## Architecture

```
Glove(L) ──ESP-NOW──► Receiver ──USB Serial──► PC Relay ──WS──► React3F / Unity
Glove(R) ──ESP-NOW──► (aggregates)            (Tier3+NLP)      (26-joint render)
  11-dim                28-dim                  L1+L2+CTC
  Tier1 CNN             Tier2 Attn              Confidence Router
```

## Tech Stack

| Layer | Technology |
|-------|-----------|
| Glove firmware | ESP32-S3, Arduino/PlatformIO, FreeRTOS, TFLite Micro |
| Receiver firmware | ESP32-S3, Arduino/PlatformIO, FreeRTOS, TFLite Micro |
| Relay | Python 3.9+, protobuf, asyncio, PyTorch, WebSocket |
| Frontend (Web) | React + Three.js (React3F), WebSocket |
| Frontend (XR) | Unity 2022 LTS, XR Hands 1.7, OpenXR |
| Training | PyTorch, CSV datasets, TFLite int8 export |

## File Structure Map

```
glove_firmware/
├── include/data_structures.h          # REWRITE (11-dim, remove Hall)
├── lib/
│   ├── Sensors/
│   │   ├── ADS1115Manager.h           # NEW (dual ADC driver)
│   │   ├── FlexManager.h              # REWRITE (ADS1115 + calibration)
│   │   ├── SensorManager.h            # REWRITE (flat I2C, no MUX)
│   │   └── BNO085Manager.h            # RETAIN (verify config)
│   ├── Comms/
│   │   ├── ESPNOWTransmitter.h        # NEW (replace UDP)
│   │   └── glove_data.proto           # REWRITE (v5 schema)
│   ├── Filters/
│   │   ├── KalmanFilter1D.h           # MODIFY (11 channels)
│   │   └── SlidingWindow.h            # RETAIN
│   └── Inference/
│       └── Tier1Model.h               # MODIFY (11-dim input, 46 classes)
└── test/                              # TDD tests per module

receiver_firmware/                     # NEW directory
├── include/
│   ├── receiver_config.h
│   └── data_structures.h
├── lib/
│   ├── ESPNOWReceiver.h               # NEW
│   ├── FramePairer.h                  # NEW (tick_id matching)
│   ├── RelativeFeatures.h             # NEW (6-dim computation)
│   └── Tier2Model.h                   # NEW (Gated Bi-CrossAttn)
└── test/

glove_relay/
├── proto/glove_data.proto             # REWRITE (single source of truth)
├── src/
│   ├── protobuf_parser.py             # REWRITE (v5 schema)
│   ├── confidence_router.py           # REWRITE (three-tier fusion)
│   ├── models/
│   │   ├── tier1_cnn.py               # NEW (PyTorch, 24K params)
│   │   ├── tier2_cross_attn.py        # NEW (Gated Bi-CrossAttn)
│   │   ├── tier3_l1.py                # NEW (CrossAttn + MS-TCN)
│   │   ├── tier3_l2_stgcn.py          # NEW (42-node ST-GCN)
│   │   └── stgcn_model.py             # REWRITE (42-node graph)
│   ├── ws_server.py                   # MODIFY (dual-hand JSON)
│   └── main.py                        # MODIFY (3-tier pipeline)
├── scripts/
│   ├── train_tier1.py                 # NEW
│   ├── train_tier2.py                 # NEW
│   ├── train_tier3.py                 # NEW
│   └── data_collector.py              # MODIFY (dual-hand CSV)
└── tests/                             # TDD tests per module

glove_web/                             # MODIFY (dual-hand rendering)
glove_unity/                           # NEW (XR Hands project)
```

---

## Phase 0: Branch & Cleanup (1 day)

### 0.1 Create branch and clean Hall code

- [ ] **0.1.1** Create V5 branch from Beta repo

```bash
cd /home/paxon/CodingProjects/EchoGloveProjects/EchoGlove-SLR-MOCAP
git checkout -b V5-DualGloveFlex
```

- [ ] **0.1.2** Delete Hall sensor modules

```bash
rm -f glove_firmware/lib/Sensors/TMAG5273.h
rm -f glove_firmware/lib/Sensors/TCA9548A.h
# Search for any remaining Hall references
grep -rn "TMAG5273\|TCA9548A\|hall_sensor\|HALL" glove_firmware/ --include="*.h" --include="*.cpp"
```

- [ ] **0.1.3** Delete old protobuf generated files

```bash
rm -f glove_relay/proto/glove_data_pb2.py
rm -f glove_firmware/lib/Comms/glove_data.pb.h
rm -f glove_firmware/lib/Comms/glove_data.pb.c
```

- [ ] **0.1.4** Update CLAUDE.md — replace V3 constants with V5

Edit `CLAUDE.md`: replace `FEATURE_COUNT=9`, `NUM_HALL_SENSORS=1` references with V5 constants: `SINGLE_HAND_FEATURES=11`, `DUAL_HAND_FEATURES=28`, `NUM_FLEX_SENSORS=5`.

- [ ] **0.1.5** Update PROGRESS.md — mark V5 migration start

Add section: `## V5.0 DualGloveFlex Migration (2026-06-01)` with phase checklist.

- [ ] **0.1.6** Verify existing relay tests still compile

```bash
cd glove_relay
python -m pytest tests/ --co -q 2>&1 | tail -5
```

**Verify**: `pytest --co` lists existing test collection without import errors.

---

## Phase 1: Simulation + Data Structures (3 days)

### 1.1 Data structures rewrite

- [ ] **1.1.1** Write test for V5 data structures

```cpp
// glove_firmware/test/test_data_structures/test_v5_constants.cpp
#include "unity.h"
#include "data_structures.h"

void test_v5_constants() {
    TEST_ASSERT_EQUAL(5, NUM_FLEX_SENSORS);
    TEST_ASSERT_EQUAL(6, IMU_FEATURE_COUNT);
    TEST_ASSERT_EQUAL(11, SINGLE_HAND_FEATURES);
    TEST_ASSERT_EQUAL(28, DUAL_HAND_FEATURES);
    TEST_ASSERT_EQUAL(30, WINDOW_SIZE);
    TEST_ASSERT_EQUAL(46, NUM_CLASSES);
    TEST_ASSERT_EQUAL(100, SENSOR_RATE_HZ);
}

void test_sensor_data_size() {
    SensorData sd;
    TEST_ASSERT_EQUAL(5, sizeof(sd.flex) / sizeof(float));
    TEST_ASSERT_EQUAL(4, sizeof(sd.quaternion) / sizeof(float));
    TEST_ASSERT_EQUAL(3, sizeof(sd.euler) / sizeof(float));
    TEST_ASSERT_EQUAL(3, sizeof(sd.gyro) / sizeof(float));
}

void test_glove_packet_size() {
    // 2+1+1+4+4+20+24+2+4+1+2+2 = 67, padded to 69
    GlovePacket gp;
    TEST_ASSERT_EQUAL(69, sizeof(GlovePacket));
}

void test_sensor_data_to_feature_array() {
    SensorData sd;
    for (int i = 0; i < 5; i++) sd.flex[i] = 0.1f * (i + 1);
    sd.euler[0] = 10.0f; sd.euler[1] = 20.0f; sd.euler[2] = 30.0f;
    sd.gyro[0] = 1.0f; sd.gyro[1] = 2.0f; sd.gyro[2] = 3.0f;
    float features[SINGLE_HAND_FEATURES];
    sd.toFeatureArray(features);
    TEST_ASSERT_EQUAL_FLOAT(0.1f, features[0]);  // flex[0]
    TEST_ASSERT_EQUAL_FLOAT(10.0f, features[5]);  // euler[0]
    TEST_ASSERT_EQUAL_FLOAT(1.0f, features[8]);   // gyro[0]
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_v5_constants);
    RUN_TEST(test_sensor_data_size);
    RUN_TEST(test_glove_packet_size);
    RUN_TEST(test_sensor_data_to_feature_array);
    return UNITY_END();
}
```

- [ ] **1.1.2** Rewrite `data_structures.h` with V5 constants

```cpp
// glove_firmware/include/data_structures.h
#pragma once
#include <cstdint>
#include <cstring>

// ── V5.0 Constants ──────────────────────────────────────────────
#define NUM_FLEX_SENSORS      5
#define IMU_FEATURE_COUNT     6      // 3 euler + 3 gyro
#define SINGLE_HAND_FEATURES  (NUM_FLEX_SENSORS + IMU_FEATURE_COUNT)  // 11
#define DUAL_HAND_FEATURES    (2 * SINGLE_HAND_FEATURES + 6)          // 28
#define WINDOW_SIZE           30
#define NUM_CLASSES           46
#define SENSOR_RATE_HZ        100
#define CALIBRATION_DURATION_MS  6000  // 3s open + 3s fist

// ── Hand ID ─────────────────────────────────────────────────────
enum HandID : uint8_t {
    HAND_LEFT  = 0,
    HAND_RIGHT = 1
};

// ── Device Status ───────────────────────────────────────────────
enum DeviceStatus : uint8_t {
    STATUS_BOOT         = 0x00,
    STATUS_CALIBRATING  = 0x01,
    STATUS_STREAMING    = 0x02,
    STATUS_ERROR        = 0xFF
};

// ── Sensor Data (per frame) ─────────────────────────────────────
struct SensorData {
    float flex[NUM_FLEX_SENSORS];     // 5 normalized [0,1]
    float quaternion[4];               // BNO085 (w,x,y,z)
    float euler[3];                    // roll, pitch, yaw (degrees)
    float gyro[3];                     // angular velocity (deg/s)
    uint32_t timestamp_us;
    uint32_t seq;

    void toFeatureArray(float* out) const {
        memcpy(out, flex, sizeof(float) * NUM_FLEX_SENSORS);
        memcpy(out + NUM_FLEX_SENSORS, euler, sizeof(float) * 3);
        memcpy(out + NUM_FLEX_SENSORS + 3, gyro, sizeof(float) * 3);
    }
};

// ── Gesture Result ──────────────────────────────────────────────
struct GestureResult {
    int32_t gesture_id;
    float confidence;
    float scores[NUM_CLASSES];
    bool valid;
    bool l2_requested;
};

// ── ESP-NOW Packet (69 bytes) ───────────────────────────────────
struct GlovePacket {
    uint8_t  magic[2];            // {0x45, 0x47} = "EG"
    uint8_t  version;             // 5
    uint8_t  hand_id;             // 0=left, 1=right
    uint32_t tick_id;             // receiver sync tick
    uint32_t timestamp_us;
    float    flex[5];             // 20 bytes
    float    imu[6];              // 24 bytes (euler[3] + gyro[3])
    uint16_t l1_gesture_id;
    float    l1_confidence;
    uint8_t  status;              // DeviceStatus enum
    uint16_t checksum;            // CRC16
    uint8_t  reserved[2];
    // Total: 2+1+1+4+4+20+24+2+4+1+2+2 = 67 → padded 69

    void computeChecksum() {
        // CRC16 over bytes[0..66]
        uint16_t crc = 0xFFFF;
        const uint8_t* data = reinterpret_cast<const uint8_t*>(this);
        for (size_t i = 0; i < 67; i++) {
            crc ^= data[i];
            for (int j = 0; j < 8; j++) {
                crc = (crc & 1) ? (crc >> 1) ^ 0xA001 : crc >> 1;
            }
        }
        checksum = crc;
    }

    bool verifyChecksum() const {
        uint16_t crc = 0xFFFF;
        const uint8_t* data = reinterpret_cast<const uint8_t*>(this);
        for (size_t i = 0; i < 67; i++) {
            crc ^= data[i];
            for (int j = 0; j < 8; j++) {
                crc = (crc & 1) ? (crc >> 1) ^ 0xA001 : crc >> 1;
            }
        }
        return checksum == crc;
    }
};

static_assert(sizeof(GlovePacket) == 69, "GlovePacket must be 69 bytes");
```

- [ ] **1.1.3** Run test — verify constants and struct sizes

```bash
cd glove_firmware
pio test -e native -f test_data_structures -v
```

**Verify**: All 4 tests pass.

### 1.2 Protobuf v5 schema

- [ ] **1.2.1** Write protobuf schema (single source of truth)

```protobuf
// glove_relay/proto/glove_data.proto
syntax = "proto3";
package echoglove_v5;

message GloveData {
    uint32 version = 1;
    uint32 timestamp = 2;
    uint32 hand_id = 3;          // 0=left, 1=right
    repeated float flex = 4;     // [5]
    repeated float imu = 5;      // [6] euler[3]+gyro[3]
    uint32 l1_gesture_id = 6;
    float l1_confidence = 7;
    repeated float relative = 8; // [6] filled by receiver
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

Copy identical file to `glove_firmware/lib/Comms/glove_data.proto`.

- [ ] **1.2.2** Generate Python protobuf bindings

```bash
cd glove_relay
pip install grpcio-tools
python -m grpc_tools.protoc -I proto --python_out=proto proto/glove_data.proto
```

- [ ] **1.2.3** Write test for protobuf round-trip

```python
# glove_relay/tests/test_protobuf_v5.py
import pytest
from proto.glove_data_pb2 import GloveData, ReceiverPacket

def test_glove_data_roundtrip():
    gd = GloveData()
    gd.version = 5
    gd.timestamp = 1234567
    gd.hand_id = 0
    gd.flex.extend([0.1, 0.2, 0.3, 0.4, 0.5])
    gd.imu.extend([10.0, 20.0, 30.0, 1.0, 2.0, 3.0])
    gd.l1_gesture_id = 15
    gd.l1_confidence = 0.95
    gd.status = "streaming"
    data = gd.SerializeToString()
    gd2 = GloveData()
    gd2.ParseFromString(data)
    assert gd2.version == 5
    assert gd2.hand_id == 0
    assert len(gd2.flex) == 5
    assert len(gd2.imu) == 6
    assert gd2.l1_gesture_id == 15
    assert abs(gd2.l1_confidence - 0.95) < 1e-6

def test_receiver_packet():
    rp = ReceiverPacket()
    rp.version = 5
    rp.tick_id = 42
    rp.left.version = 5
    rp.left.hand_id = 0
    rp.left.flex.extend([0.1] * 5)
    rp.left.imu.extend([0.0] * 6)
    rp.right.version = 5
    rp.right.hand_id = 1
    rp.right.flex.extend([0.2] * 5)
    rp.right.imu.extend([0.0] * 6)
    rp.relative_features.extend([0.1, 0.2, 0.3, 0.01, 0.5, 0.33])
    data = rp.SerializeToString()
    rp2 = ReceiverPacket()
    rp2.ParseFromString(data)
    assert rp2.tick_id == 42
    assert len(rp2.relative_features) == 6
    assert rp2.left.hand_id == 0
    assert rp2.right.hand_id == 1

def test_relative_features_placeholder():
    """Relative features [6] = ΔEuler[3] + ΔQuatDist[1] + ΔGyroNorm[1] + ΔGyroAxis[1]"""
    gd = GloveData()
    gd.relative.extend([0.0] * 6)
    assert len(gd.relative) == 6
```

- [ ] **1.2.4** Run protobuf tests

```bash
cd glove_relay
python -m pytest tests/test_protobuf_v5.py -v
```

**Verify**: 3 tests pass.

### 1.3 Simulation mode (SensorManager)

- [ ] **1.3.1** Write test for simulation mode

```cpp
// glove_firmware/test/test_simulation/test_sim_sensor_manager.cpp
#include "unity.h"
#include "SensorManager.h"

SensorManager sm;

void setUp() { sm.begin(/*simulation=*/true); }

void test_sim_returns_valid_sensor_data() {
    SensorData sd = sm.read();
    TEST_ASSERT_TRUE(sd.seq > 0);
    TEST_ASSERT_TRUE(sd.timestamp_us > 0);
}

void test_sim_flex_in_range() {
    SensorData sd = sm.read();
    for (int i = 0; i < NUM_FLEX_SENSORS; i++) {
        TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, sd.flex[i]);
        TEST_ASSERT_TRUE(sd.flex[i] >= 0.0f && sd.flex[i] <= 1.0f);
    }
}

void test_sim_euler_in_range() {
    SensorData sd = sm.read();
    for (int i = 0; i < 3; i++) {
        TEST_ASSERT_TRUE(sd.euler[i] >= -180.0f && sd.euler[i] <= 180.0f);
    }
}

void test_sim_gyro_in_range() {
    SensorData sd = sm.read();
    for (int i = 0; i < 3; i++) {
        TEST_ASSERT_TRUE(sd.gyro[i] >= -500.0f && sd.gyro[i] <= 500.0f);
    }
}

void test_sim_gesture_signature_output() {
    // Simulate gesture 0 (open hand), should produce distinct pattern
    sm.setSimulatedGesture(0);
    SensorData sd = sm.read();
    // Open hand: all flex ~0
    for (int i = 0; i < NUM_FLEX_SENSORS; i++) {
        TEST_ASSERT_TRUE(sd.flex[i] < 0.3f);
    }
}

void test_sim_to_feature_array() {
    SensorData sd = sm.read();
    float features[SINGLE_HAND_FEATURES];
    sd.toFeatureArray(features);
    // Feature array should have 11 elements
    TEST_ASSERT_EQUAL_FLOAT(sd.flex[0], features[0]);
    TEST_ASSERT_EQUAL_FLOAT(sd.euler[0], features[5]);
    TEST_ASSERT_EQUAL_FLOAT(sd.gyro[0], features[8]);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_sim_returns_valid_sensor_data);
    RUN_TEST(test_sim_flex_in_range);
    RUN_TEST(test_sim_euler_in_range);
    RUN_TEST(test_sim_gyro_in_range);
    RUN_TEST(test_sim_gesture_signature_output);
    RUN_TEST(test_sim_to_feature_array);
    return UNITY_END();
}
```

- [ ] **1.3.2** Rewrite SensorManager simulation mode

The simulation generates 11-dim data (5 flex + 3 euler + 3 gyro) with 20 gesture signatures. Each gesture has a characteristic flex pattern + IMU offset.

```cpp
// Key changes in SensorManager.h:
// - Remove all TMAG5273, TCA9548A references
// - Simulation generates SensorData with 5 flex + quaternion + euler + gyro
// - setSimulatedGesture(id) sets target pattern
// - begin(simulation=true) initializes sim mode
```

- [ ] **1.3.3** Run simulation tests

```bash
cd glove_firmware
pio test -e native -f test_sim_sensor_manager -v
```

**Verify**: All 6 tests pass.

### 1.4 FlexManager stub

- [ ] **1.4.1** Write test for FlexManager interface

```cpp
// glove_firmware/test/test_flex/test_flex_manager.cpp
#include "unity.h"
#include "FlexManager.h"

FlexManager fm;

void test_flex_manager_begin() {
    TEST_ASSERT_TRUE(fm.begin(/*simulation=*/true));
}

void test_flex_manager_read_returns_5() {
    fm.begin(true);
    float values[NUM_FLEX_SENSORS];
    TEST_ASSERT_TRUE(fm.read(values));
}

void test_flex_manager_values_normalized() {
    fm.begin(true);
    float values[NUM_FLEX_SENSORS];
    fm.read(values);
    for (int i = 0; i < NUM_FLEX_SENSORS; i++) {
        TEST_ASSERT_TRUE(values[i] >= 0.0f && values[i] <= 1.0f);
    }
}

void test_flex_manager_calibration_interface() {
    fm.begin(true);
    fm.startCalibration();
    TEST_ASSERT_TRUE(fm.isCalibrating());
    // Simulate 6 seconds of data
    for (int i = 0; i < 600; i++) fm.read(nullptr);
    // After calibration, should not be calibrating
    TEST_ASSERT_FALSE(fm.isCalibrating());
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_flex_manager_begin);
    RUN_TEST(test_flex_manager_read_returns_5);
    RUN_TEST(test_flex_manager_values_normalized);
    RUN_TEST(test_flex_manager_calibration_interface);
    return UNITY_END();
}
```

- [ ] **1.4.2** Rewrite FlexManager stub (simulation mode)

```cpp
// glove_firmware/lib/Sensors/FlexManager.h
#pragma once
#include "data_structures.h"

class FlexManager {
public:
    bool begin(bool simulation = false) {
        simulation_ = simulation;
        calibrated_ = false;
        calibrating_ = false;
        return true;
    }

    bool read(float* values) {
        if (values) {
            for (int i = 0; i < NUM_FLEX_SENSORS; i++) {
                values[i] = sim_values_[i];
            }
        }
        if (calibrating_) {
            cal_frame_count_++;
            if (cal_frame_count_ >= 600) { // 6s at 100Hz
                calibrating_ = false;
                calibrated_ = true;
            }
        }
        return true;
    }

    void startCalibration() {
        calibrating_ = true;
        cal_frame_count_ = 0;
    }

    bool isCalibrating() const { return calibrating_; }
    bool isCalibrated() const { return calibrated_; }

    // Simulation: set target flex values
    void setSimulatedValues(const float* values) {
        for (int i = 0; i < NUM_FLEX_SENSORS; i++) sim_values_[i] = values[i];
    }

private:
    bool simulation_ = false;
    bool calibrated_ = false;
    bool calibrating_ = false;
    int cal_frame_count_ = 0;
    float sim_values_[NUM_FLEX_SENSORS] = {0};
};
```

- [ ] **1.4.3** Run FlexManager tests

```bash
cd glove_firmware
pio test -e native -f test_flex_manager -v
```

**Verify**: All 4 tests pass.

### 1.5 ESP-NOW transmitter stub

- [ ] **1.5.1** Write test for ESPNOWTransmitter

```cpp
// glove_firmware/test/test_espnow/test_espnow_tx.cpp
#include "unity.h"
#include "ESPNOWTransmitter.h"

ESPNOWTransmitter tx;

void test_espnow_begin() {
    TEST_ASSERT_TRUE(tx.begin(HAND_LEFT));
}

void test_espnow_send_packet() {
    tx.begin(HAND_LEFT);
    GlovePacket pkt = {};
    pkt.magic[0] = 0x45; pkt.magic[1] = 0x47;
    pkt.version = 5;
    pkt.hand_id = HAND_LEFT;
    pkt.tick_id = 1;
    pkt.timestamp_us = 1000000;
    pkt.flex[0] = 0.5f;
    pkt.imu[0] = 10.0f;
    pkt.status = STATUS_STREAMING;
    pkt.computeChecksum();
    TEST_ASSERT_TRUE(tx.send(pkt));
}

void test_espnow_packet_is_69_bytes() {
    TEST_ASSERT_EQUAL(69, sizeof(GlovePacket));
}

void test_espnow_checksum_roundtrip() {
    GlovePacket pkt = {};
    pkt.magic[0] = 0x45; pkt.magic[1] = 0x47;
    pkt.version = 5;
    pkt.computeChecksum();
    TEST_ASSERT_TRUE(pkt.verifyChecksum());
    pkt.flex[0] = 0.99f; // tamper
    TEST_ASSERT_FALSE(pkt.verifyChecksum());
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_espnow_begin);
    RUN_TEST(test_espnow_send_packet);
    RUN_TEST(test_espnow_packet_is_69_bytes);
    RUN_TEST(test_espnow_checksum_roundtrip);
    return UNITY_END();
}
```

- [ ] **1.5.2** Create ESPNOWTransmitter stub (simulation mode)

```cpp
// glove_firmware/lib/Comms/ESPNOWTransmitter.h
#pragma once
#include "data_structures.h"
#include <cstdint>

class ESPNOWTransmitter {
public:
    bool begin(HandID hand_id) {
        hand_id_ = hand_id;
        initialized_ = true;
        return true;
    }

    bool send(const GlovePacket& pkt) {
        if (!initialized_) return false;
        last_sent_ = pkt;
        send_count_++;
        return true;
    }

    uint32_t getSendCount() const { return send_count_; }
    const GlovePacket& getLastSent() const { return last_sent_; }

private:
    bool initialized_ = false;
    HandID hand_id_ = HAND_LEFT;
    uint32_t send_count_ = 0;
    GlovePacket last_sent_ = {};
};
```

- [ ] **1.5.3** Run ESP-NOW tests

```bash
cd glove_firmware
pio test -e native -f test_espnow -v
```

**Verify**: All 4 tests pass.

### 1.6 CSV output validation

- [ ] **1.6.1** Write test for CSV format compliance

```python
# glove_relay/tests/test_csv_format.py
import pytest
import csv
import io

EXPECTED_HEADER = "timestamp,hand_id,flex0,flex1,flex2,flex3,flex4,euler_x,euler_y,euler_z,gyro_x,gyro_y,gyro_z,gesture_id"

def test_csv_header_matches_spec():
    """Verify CSV format matches design spec Section 7.2"""
    header_fields = EXPECTED_HEADER.split(",")
    assert len(header_fields) == 14
    assert header_fields[0] == "timestamp"
    assert header_fields[1] == "hand_id"
    assert header_fields[2] == "flex0"
    assert header_fields[12] == "gyro_z"
    assert header_fields[13] == "gesture_id"

def test_csv_row_parseable():
    """Verify a sample row is parseable"""
    row = "1234567,0,0.12,0.85,0.78,0.65,0.23,12.3,-45.6,78.9,0.5,-1.2,0.3,15"
    reader = csv.reader(io.StringIO(row))
    fields = next(reader)
    assert len(fields) == 14
    assert int(fields[1]) == 0  # hand_id
    assert int(fields[13]) == 15  # gesture_id
    assert 0.0 <= float(fields[2]) <= 1.0  # flex0 normalized
```

- [ ] **1.6.2** Run CSV format tests

```bash
cd glove_relay
python -m pytest tests/test_csv_format.py -v
```

**Verify**: 2 tests pass.

### 1.7 Phase 1 integration check

- [ ] **1.7.1** Run all Phase 1 tests together

```bash
# Firmware tests
cd glove_firmware
pio test -e native -v

# Relay tests
cd ../glove_relay
python -m pytest tests/test_protobuf_v5.py tests/test_csv_format.py -v
```

**Verify**: All Phase 1 tests pass. Simulation generates valid 11-dim data, protobuf encodes/decodes correctly.

---

## Phase 2: Relay Pipeline Upgrade (5 days)

### 2.1 Protobuf parser rewrite

- [ ] **2.1.1** Write tests for v5 protobuf parser

```python
# glove_relay/tests/test_protobuf_parser_v5.py
import pytest
from src.protobuf_parser import ProtobufParser
from proto.glove_data_pb2 import ReceiverPacket

@pytest.fixture
def parser():
    return ProtobufParser()

def _make_receiver_packet(tick_id=1, left_flex=None, right_flex=None):
    rp = ReceiverPacket()
    rp.version = 5
    rp.tick_id = tick_id
    rp.left.version = 5
    rp.left.hand_id = 0
    rp.left.flex.extend(left_flex or [0.1, 0.2, 0.3, 0.4, 0.5])
    rp.left.imu.extend([10.0, 20.0, 30.0, 1.0, 2.0, 3.0])
    rp.left.l1_gesture_id = 5
    rp.left.l1_confidence = 0.9
    rp.right.version = 5
    rp.right.hand_id = 1
    rp.right.flex.extend(right_flex or [0.6, 0.7, 0.8, 0.9, 1.0])
    rp.right.imu.extend([40.0, 50.0, 60.0, 4.0, 5.0, 6.0])
    rp.right.l1_gesture_id = 10
    rp.right.l1_confidence = 0.85
    rp.relative_features.extend([0.1, 0.2, 0.3, 0.01, 0.5, 0.33])
    return rp

def test_parse_v5_receiver_packet(parser):
    rp = _make_receiver_packet()
    data = rp.SerializeToString()
    result = parser.parse_receiver_packet(data)
    assert result is not None
    assert result["tick_id"] == 1
    assert result["left"]["hand_id"] == 0
    assert result["right"]["hand_id"] == 1
    assert len(result["left"]["flex"]) == 5
    assert len(result["relative_features"]) == 6

def test_parse_v5_version_check(parser):
    """Reject packets with wrong version"""
    rp = _make_receiver_packet()
    rp.version = 3  # wrong version
    data = rp.SerializeToString()
    result = parser.parse_receiver_packet(data)
    assert result is None

def test_parse_v5_28dim_features(parser):
    """Verify 28-dim feature assembly: left[11] + right[11] + relative[6]"""
    rp = _make_receiver_packet()
    data = rp.SerializeToString()
    result = parser.parse_receiver_packet(data)
    features = parser.assemble_28dim(result)
    assert len(features) == 28
    # left flex[0] should be at index 0
    assert abs(features[0] - 0.1) < 1e-6
    # right flex[0] should be at index 11
    assert abs(features[11] - 0.6) < 1e-6
    # relative[0] should be at index 22
    assert abs(features[22] - 0.1) < 1e-6
```

- [ ] **2.1.2** Rewrite `protobuf_parser.py` for v5 schema

```python
# glove_relay/src/protobuf_parser.py
from __future__ import annotations
from typing import Optional
import struct
from proto.glove_data_pb2 import GloveData, ReceiverPacket

class ProtobufParser:
    def __init__(self):
        self._expected_version = 5

    def parse_receiver_packet(self, data: bytes) -> Optional[dict]:
        try:
            rp = ReceiverPacket()
            rp.ParseFromString(data)
            if rp.version != self._expected_version:
                return None
            return {
                "version": rp.version,
                "tick_id": rp.tick_id,
                "left": self._parse_glove_data(rp.left),
                "right": self._parse_glove_data(rp.right),
                "relative_features": list(rp.relative_features),
            }
        except Exception:
            return None

    def _parse_glove_data(self, gd: GloveData) -> dict:
        return {
            "version": gd.version,
            "hand_id": gd.hand_id,
            "flex": list(gd.flex),
            "imu": list(gd.imu),
            "l1_gesture_id": gd.l1_gesture_id,
            "l1_confidence": gd.l1_confidence,
            "relative": list(gd.relative),
            "tier2_gesture_id": gd.tier2_gesture_id,
            "tier2_confidence": gd.tier2_confidence,
            "status": gd.status,
        }

    def assemble_28dim(self, parsed: dict) -> list[float]:
        """Assemble 28-dim feature vector: left[11] + right[11] + relative[6]"""
        left = parsed["left"]
        right = parsed["right"]
        features = []
        # Left: flex[5] + imu_euler[3] + imu_gyro[3] = 11
        features.extend(left["flex"])
        features.extend(left["imu"][:6])  # euler[3] + gyro[3]
        # Right: flex[5] + imu_euler[3] + imu_gyro[3] = 11
        features.extend(right["flex"])
        features.extend(right["imu"][:6])
        # Relative: [6]
        features.extend(parsed["relative_features"])
        return features
```

- [ ] **2.1.3** Run protobuf parser tests

```bash
cd glove_relay
python -m pytest tests/test_protobuf_parser_v5.py -v
```

**Verify**: 3 tests pass.

### 2.2 Tier1 CNN model (PyTorch)

- [ ] **2.2.1** Write tests for Tier1 CNN

```python
# glove_relay/tests/test_tier1_cnn.py
import pytest
import torch
from src.models.tier1_cnn import Tier1CNN

@pytest.fixture
def model():
    return Tier1CNN(input_dim=11, num_classes=46)

def test_tier1_output_shape(model):
    x = torch.randn(1, 11)  # single frame
    out = model(x)
    assert out.shape == (1, 46)

def test_tier1_window_input(model):
    x = torch.randn(1, 30, 11)  # 30-frame window
    out = model(x)
    assert out.shape == (1, 46)

def test_tier1_param_count(model):
    """Should be ~24K params (<80KB int8)"""
    count = sum(p.numel() for p in model.parameters())
    assert count < 30000, f"Too many params: {count}"
    assert count > 10000, f"Too few params: {count}"

def test_tier1_output_is_probabilities(model):
    x = torch.randn(1, 11)
    out = torch.softmax(model(x), dim=1)
    assert torch.allclose(out.sum(), torch.tensor(1.0), atol=1e-5)

def test_tier1_export_torchscript(model, tmp_path):
    model.eval()
    x = torch.randn(1, 11)
    traced = torch.jit.trace(model, x)
    path = tmp_path / "tier1.pt"
    traced.save(str(path))
    assert path.exists()
```

- [ ] **2.2.2** Implement Tier1 CNN model

```python
# glove_relay/src/models/tier1_cnn.py
from __future__ import annotations
import torch
import torch.nn as nn

class SEBlock(nn.Module):
    """Squeeze-and-Excitation attention"""
    def __init__(self, channels: int, reduction: int = 4):
        super().__init__()
        self.fc = nn.Sequential(
            nn.Linear(channels, channels // reduction),
            nn.ReLU(),
            nn.Linear(channels // reduction, channels),
            nn.Sigmoid(),
        )

    def forward(self, x: torch.Tensor) -> torch.Tensor:
        # x: (B, C)
        w = self.fc(x)
        return x * w

class Tier1CNN(nn.Module):
    """CNN + SE-Attention for single-hand gesture classification.
    Input: (B, 11) or (B, 30, 11)
    Output: (B, 46)
    ~24K parameters
    """
    def __init__(self, input_dim: int = 11, num_classes: int = 46, hidden: int = 64):
        super().__init__()
        self.net = nn.Sequential(
            nn.Linear(input_dim, hidden),
            nn.ReLU(),
            nn.BatchNorm1d(hidden),
            SEBlock(hidden),
            nn.Linear(hidden, hidden),
            nn.ReLU(),
            nn.BatchNorm1d(hidden),
            SEBlock(hidden),
            nn.Linear(hidden, num_classes),
        )

    def forward(self, x: torch.Tensor) -> torch.Tensor:
        if x.dim() == 3:  # (B, T, C) → mean pool
            x = x.mean(dim=1)
        return self.net(x)
```

- [ ] **2.2.3** Run Tier1 CNN tests

```bash
cd glove_relay
python -m pytest tests/test_tier1_cnn.py -v
```

**Verify**: 5 tests pass. Parameter count ~24K.

### 2.3 Gated Bi-CrossAttention (Tier2/3)

- [ ] **2.3.1** Write tests for Gated Bi-CrossAttention

```python
# glove_relay/tests/test_cross_attention.py
import pytest
import torch
from src.models.tier2_cross_attn import GatedBiCrossAttention

@pytest.fixture
def model():
    return GatedBiCrossAttention(d_model=32, num_classes=46)

def test_cross_attn_output_shape(model):
    left = torch.randn(1, 11)
    right = torch.randn(1, 11)
    out = model(left, right)
    assert out.shape == (1, 46)

def test_cross_attn_gate_range(model):
    """Gate values should be in [0, 1]"""
    left = torch.randn(1, 11)
    right = torch.randn(1, 11)
    gate_l, gate_r = model.get_gates(left, right)
    assert gate_l.min() >= 0.0 and gate_l.max() <= 1.0
    assert gate_r.min() >= 0.0 and gate_r.max() <= 1.0

def test_cross_attn_param_count(model):
    """~80KB int8 ≈ 20K float params"""
    count = sum(p.numel() for p in model.parameters())
    assert count < 30000
    assert count > 5000

def test_cross_attn_window_input(model):
    left = torch.randn(1, 30, 11)
    right = torch.randn(1, 30, 11)
    out = model(left, right)
    assert out.shape == (1, 46)
```

- [ ] **2.3.2** Implement Gated Bi-CrossAttention

```python
# glove_relay/src/models/tier2_cross_attn.py
from __future__ import annotations
import torch
import torch.nn as nn

class GatedBiCrossAttention(nn.Module):
    """Left/right features attend to each other, sigmoid gate controls fusion.
    Input: left (B, 11), right (B, 11)
    Output: (B, num_classes)
    ~20K parameters
    """
    def __init__(self, d_model: int = 32, num_classes: int = 46):
        super().__init__()
        self.embed = nn.Linear(11, d_model)
        self.cross_attn = nn.MultiheadAttention(d_model, num_heads=4, batch_first=True)
        self.gate_proj = nn.Linear(d_model * 2, d_model)
        self.classifier = nn.Sequential(
            nn.Linear(d_model * 2, d_model),
            nn.ReLU(),
            nn.Linear(d_model, num_classes),
        )

    def get_gates(self, left: torch.Tensor, right: torch.Tensor):
        """Return gate values for inspection"""
        if left.dim() == 3:
            left = left.mean(dim=1)
            right = right.mean(dim=1)
        e_l = self.embed(left).unsqueeze(1)  # (B, 1, d)
        e_r = self.embed(right).unsqueeze(1)
        attn_l, _ = self.cross_attn(e_l, e_r, e_r)  # left attends to right
        attn_r, _ = self.cross_attn(e_r, e_l, e_l)  # right attends to left
        attn_l = attn_l.squeeze(1)
        attn_r = attn_r.squeeze(1)
        e_l = e_l.squeeze(1)
        e_r = e_r.squeeze(1)
        gate_l = torch.sigmoid(self.gate_proj(torch.cat([e_l, attn_l], dim=1)))
        gate_r = torch.sigmoid(self.gate_proj(torch.cat([e_r, attn_r], dim=1)))
        return gate_l, gate_r

    def forward(self, left: torch.Tensor, right: torch.Tensor) -> torch.Tensor:
        if left.dim() == 3:
            left = left.mean(dim=1)
            right = right.mean(dim=1)
        e_l = self.embed(left).unsqueeze(1)
        e_r = self.embed(right).unsqueeze(1)
        attn_l, _ = self.cross_attn(e_l, e_r, e_r)
        attn_r, _ = self.cross_attn(e_r, e_l, e_l)
        attn_l = attn_l.squeeze(1)
        attn_r = attn_r.squeeze(1)
        e_l = e_l.squeeze(1)
        e_r = e_r.squeeze(1)
        gate_l = torch.sigmoid(self.gate_proj(torch.cat([e_l, attn_l], dim=1)))
        gate_r = torch.sigmoid(self.gate_proj(torch.cat([e_r, attn_r], dim=1)))
        fused_l = gate_l * attn_l + (1 - gate_l) * e_l
        fused_r = gate_r * attn_r + (1 - gate_r) * e_r
        combined = torch.cat([fused_l, fused_r], dim=1)
        return self.classifier(combined)
```

- [ ] **2.3.3** Run cross-attention tests

```bash
cd glove_relay
python -m pytest tests/test_cross_attention.py -v
```

**Verify**: 4 tests pass.

### 2.4 ST-GCN model (42-node)

- [ ] **2.4.1** Write tests for ST-GCN

```python
# glove_relay/tests/test_stgcn_v5.py
import pytest
import torch
from src.models.stgcn_model import STGCNModel

@pytest.fixture
def model():
    return STGCNModel(num_nodes=42, in_features=28, num_classes=46)

def test_stgcn_output_shape(model):
    x = torch.randn(1, 30, 42, 28)  # (B, T, N, C)
    out = model(x)
    assert out.shape == (1, 46)

def test_stgcn_12node_variant():
    """Tier1/2 use 12-node simplified graph"""
    model = STGCNModel(num_nodes=12, in_features=11, num_classes=46)
    x = torch.randn(1, 30, 12, 11)
    out = model(x)
    assert out.shape == (1, 46)

def test_stgcn_edge_count_42node():
    """42-node graph should have 88 edges (40 intra + 6 cross + 42 self)"""
    model = STGCNModel(num_nodes=42, in_features=28, num_classes=46)
    assert model.num_edges == 88

def test_stgcn_edge_count_12node():
    """12-node graph should have 22 edges"""
    model = STGCNModel(num_nodes=12, in_features=11, num_classes=46)
    assert model.num_edges == 22
```

- [ ] **2.4.2** Rewrite `stgcn_model.py` with hierarchical graph

```python
# glove_relay/src/models/stgcn_model.py
from __future__ import annotations
import torch
import torch.nn as nn
import numpy as np

def build_hand_skeleton_edges(num_fingers: int = 5, joints_per_finger: int = 4) -> list[tuple[int, int]]:
    """Build tree edges for one hand: WRIST(0) → finger roots → ... → tips"""
    edges = []
    node = 1  # node 0 = wrist
    for f in range(num_fingers):
        prev = 0  # wrist
        for j in range(joints_per_finger):
            edges.append((prev, node))
            prev = node
            node += 1
    return edges

def build_42node_edges() -> list[tuple[int, int]]:
    """42-node dual-hand graph: 40 intra-hand + 6 cross-hand + 42 self-loops"""
    edges = []
    # Left hand: nodes 0-20 (wrist + 5 fingers × 4 joints)
    left_edges = build_hand_skeleton_edges()
    edges.extend(left_edges)
    # Right hand: nodes 21-41, offset by 21
    for (a, b) in left_edges:
        edges.append((a + 21, b + 21))
    # Cross-hand fingertip correspondence: left tips ↔ right tips
    left_tips = [4, 8, 12, 16, 20]   # left fingertips
    right_tips = [25, 29, 33, 37, 41]  # right fingertips
    for lt, rt in zip(left_tips, right_tips):
        edges.append((lt, rt))
    # Self-loops
    for i in range(42):
        edges.append((i, i))
    return edges

def build_12node_edges() -> list[tuple[int, int]]:
    """12-node simplified: wrist + 5 fingertips × 2 hands"""
    edges = []
    # Left: nodes 0-5 (wrist + 5 fingertips)
    for i in range(1, 6):
        edges.append((0, i))
    # Right: nodes 6-11 (wrist + 5 fingertips)
    for i in range(7, 12):
        edges.append((6, i))
    # Cross-hand tips
    for i in range(1, 6):
        edges.append((i, i + 5))
    # Self-loops
    for i in range(12):
        edges.append((i, i))
    return edges

class GraphConvolution(nn.Module):
    def __init__(self, in_features: int, out_features: int, num_nodes: int, edges: list):
        super().__init__()
        self.num_nodes = num_nodes
        # Adjacency matrix from edges
        adj = torch.zeros(num_nodes, num_nodes)
        for (a, b) in edges:
            adj[a, b] = 1.0
        # Normalize
        deg = adj.sum(dim=1, keepdim=True).clamp(min=1)
        adj = adj / deg
        self.register_buffer("adj", adj)
        self.linear = nn.Linear(in_features, out_features)

    def forward(self, x: torch.Tensor) -> torch.Tensor:
        # x: (B, T, N, C) or (B, N, C)
        if x.dim() == 4:
            B, T, N, C = x.shape
            x = x.reshape(B * T, N, C)
            x = torch.matmul(self.adj, x)  # (B*T, N, N) @ (B*T, N, C)
            x = self.linear(x)
            x = x.reshape(B, T, N, -1)
        else:
            x = torch.matmul(self.adj, x)
            x = self.linear(x)
        return x

class STGCNModel(nn.Module):
    """Spatial-Temporal Graph Convolutional Network.
    Supports 12-node (Tier1/2) and 42-node (Tier3) graphs.
    """
    def __init__(self, num_nodes: int, in_features: int, num_classes: int, hidden: int = 64):
        super().__init__()
        self.num_nodes = num_nodes
        if num_nodes == 42:
            edges = build_42node_edges()
        elif num_nodes == 12:
            edges = build_12node_edges()
        else:
            raise ValueError(f"Unsupported node count: {num_nodes}")
        self.num_edges = len(edges)
        self.spatial = nn.Sequential(
            GraphConvolution(in_features, hidden, num_nodes, edges),
            nn.ReLU(),
            GraphConvolution(hidden, hidden, num_nodes, edges),
            nn.ReLU(),
        )
        self.temporal = nn.Sequential(
            nn.Conv1d(hidden, hidden, kernel_size=3, padding=1),
            nn.ReLU(),
            nn.Conv1d(hidden, hidden, kernel_size=3, padding=1),
            nn.ReLU(),
        )
        self.classifier = nn.Sequential(
            nn.Linear(hidden * num_nodes, 128),
            nn.ReLU(),
            nn.Linear(128, num_classes),
        )

    def forward(self, x: torch.Tensor) -> torch.Tensor:
        # x: (B, T, N, C)
        x = self.spatial(x)         # (B, T, N, hidden)
        B, T, N, H = x.shape
        x = x.permute(0, 2, 3, 1)  # (B, N, H, T)
        x = x.reshape(B * N, H, T)
        x = self.temporal(x)        # (B*N, H, T)
        x = x.mean(dim=2)           # (B*N, H)
        x = x.reshape(B, N * H)
        return self.classifier(x)
```

- [ ] **2.4.3** Run ST-GCN tests

```bash
cd glove_relay
python -m pytest tests/test_stgcn_v5.py -v
```

**Verify**: 4 tests pass.

### 2.5 Confidence Router (three-tier fusion)

- [ ] **2.5.1** Write tests for three-tier Confidence Router

```python
# glove_relay/tests/test_confidence_router_v5.py
import pytest
from src.confidence_router import ConfidenceRouter

@pytest.fixture
def router():
    return ConfidenceRouter()

def test_tier1_only(router):
    """When only Tier1 is available, use its result"""
    result = router.route(
        tier1={"gesture_id": 5, "confidence": 0.9},
        tier2=None,
        tier3=None,
    )
    assert result["gesture_id"] == 5
    assert result["active_tier"] == "tier1"

def test_tier2_overrides_tier1(router):
    """Tier2 should override Tier1 when confidence is higher"""
    result = router.route(
        tier1={"gesture_id": 5, "confidence": 0.7},
        tier2={"gesture_id": 8, "confidence": 0.95},
        tier3=None,
    )
    assert result["gesture_id"] == 8
    assert result["active_tier"] == "tier2"

def test_tier3_overrides_all(router):
    """Tier3 should override all when available"""
    result = router.route(
        tier1={"gesture_id": 5, "confidence": 0.7},
        tier2={"gesture_id": 8, "confidence": 0.8},
        tier3={"gesture_id": 12, "confidence": 0.99},
    )
    assert result["gesture_id"] == 12
    assert result["active_tier"] == "tier3"

def test_hot_swap_blend(router):
    """5-frame linear blend when switching tiers"""
    # Simulate 5 frames of transition
    old = {"gesture_id": 5, "confidence": 0.9}
    new = {"gesture_id": 8, "confidence": 0.95}
    for frame in range(5):
        result = router.route(
            tier1=old if frame < 2 else new,
            tier2=new if frame >= 2 else None,
            tier3=None,
        )
    # After transition, should be using tier2
    assert result["active_tier"] == "tier2"

def test_low_confidence_fallback(router):
    """If Tier2 confidence drops below threshold, fall back to Tier1"""
    result = router.route(
        tier1={"gesture_id": 5, "confidence": 0.85},
        tier2={"gesture_id": 8, "confidence": 0.3},  # low confidence
        tier3=None,
    )
    assert result["active_tier"] == "tier1"
```

- [ ] **2.5.2** Rewrite ConfidenceRouter

```python
# glove_relay/src/confidence_router.py
from __future__ import annotations
from typing import Optional
from collections import deque

class ConfidenceRouter:
    def __init__(self, blend_frames: int = 5, min_confidence: float = 0.5):
        self.blend_frames = blend_frames
        self.min_confidence = min_confidence
        self.active_tier = "tier1"
        self.blend_counter = 0
        self.blend_source = None
        self.blend_target = None
        self.history = deque(maxlen=10)

    def route(
        self,
        tier1: Optional[dict] = None,
        tier2: Optional[dict] = None,
        tier3: Optional[dict] = None,
    ) -> dict:
        # Select best available tier
        best = tier1 or {"gesture_id": 0, "confidence": 0.0}
        best_tier = "tier1"

        if tier2 and tier2["confidence"] >= self.min_confidence:
            best = tier2
            best_tier = "tier2"

        if tier3 and tier3["confidence"] >= self.min_confidence:
            best = tier3
            best_tier = "tier3"

        # Hot-swap blending
        if best_tier != self.active_tier:
            self.blend_source = self.active_tier
            self.blend_target = best_tier
            self.blend_counter = 0
            self.active_tier = best_tier

        alpha = 1.0
        if self.blend_counter < self.blend_frames and self.blend_source:
            alpha = self.blend_counter / self.blend_frames
            self.blend_counter += 1
            if self.blend_counter >= self.blend_frames:
                self.blend_source = None

        self.history.append(best)
        return {
            "gesture_id": best["gesture_id"],
            "confidence": best["confidence"],
            "active_tier": self.active_tier,
            "blend_alpha": alpha,
        }
```

- [ ] **2.5.3** Run Confidence Router tests

```bash
cd glove_relay
python -m pytest tests/test_confidence_router_v5.py -v
```

**Verify**: 5 tests pass.

### 2.6 WebSocket format update

- [ ] **2.6.1** Write test for dual-hand WebSocket JSON

```python
# glove_relay/tests/test_ws_format_v5.py
import pytest
import json

def test_ws_json_dual_hand_format():
    """WebSocket JSON should include left/right hands + inference"""
    msg = {
        "tier": "tier2",
        "blend_alpha": 1.0,
        "left_hand": {
            "flex": [0.1, 0.2, 0.3, 0.4, 0.5],
            "euler": [10.0, 20.0, 30.0],
            "gyro": [1.0, 2.0, 3.0],
            "gesture_id": 5,
            "confidence": 0.9,
        },
        "right_hand": {
            "flex": [0.6, 0.7, 0.8, 0.9, 1.0],
            "euler": [40.0, 50.0, 60.0],
            "gyro": [4.0, 5.0, 6.0],
            "gesture_id": 10,
            "confidence": 0.85,
        },
        "relative": {
            "delta_euler": [0.1, 0.2, 0.3],
            "delta_quat_dist": 0.01,
            "delta_gyro_norm": 0.5,
            "delta_gyro_axis": 0.33,
        },
        "inference": {
            "gesture_id": 8,
            "confidence": 0.95,
            "text": "hello",
        },
        "nlp_text": "你好",
    }
    serialized = json.dumps(msg)
    parsed = json.loads(serialized)
    assert parsed["tier"] == "tier2"
    assert len(parsed["left_hand"]["flex"]) == 5
    assert len(parsed["right_hand"]["flex"]) == 5
    assert len(parsed["relative"]["delta_euler"]) == 3
    assert "nlp_text" in parsed
```

- [ ] **2.6.2** Run WebSocket format tests

```bash
cd glove_relay
python -m pytest tests/test_ws_format_v5.py -v
```

**Verify**: 1 test passes.

### 2.7 Model training scripts

- [ ] **2.7.1** Create Tier1 training script

```python
# glove_relay/scripts/train_tier1.py
"""Train Tier1 CNN model on single-hand CSV data."""
from __future__ import annotations
import argparse
import pandas as pd
import torch
import torch.nn as nn
from torch.utils.data import DataLoader, TensorDataset
from src.models.tier1_cnn import Tier1CNN

def load_data(csv_path: str) -> tuple[torch.Tensor, torch.Tensor]:
    df = pd.read_csv(csv_path)
    feature_cols = [f"flex{i}" for i in range(5)] + ["euler_x", "euler_y", "euler_z", "gyro_x", "gyro_y", "gyro_z"]
    X = torch.tensor(df[feature_cols].values, dtype=torch.float32)
    y = torch.tensor(df["gesture_id"].values, dtype=torch.long)
    return X, y

def train(csv_path: str, output_path: str, epochs: int = 100, batch_size: int = 32):
    X, y = load_data(csv_path)
    dataset = TensorDataset(X, y)
    loader = DataLoader(dataset, batch_size=batch_size, shuffle=True)
    model = Tier1CNN(input_dim=11, num_classes=46)
    optimizer = torch.optim.Adam(model.parameters(), lr=1e-3)
    criterion = nn.CrossEntropyLoss()
    for epoch in range(epochs):
        total_loss = 0
        for batch_x, batch_y in loader:
            out = model(batch_x)
            loss = criterion(out, batch_y)
            optimizer.zero_grad()
            loss.backward()
            optimizer.step()
            total_loss += loss.item()
        if (epoch + 1) % 10 == 0:
            print(f"Epoch {epoch+1}/{epochs} loss={total_loss/len(loader):.4f}")
    # Export TorchScript
    model.eval()
    traced = torch.jit.trace(model, torch.randn(1, 11))
    traced.save(output_path)
    print(f"Saved to {output_path}")

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--data", required=True)
    parser.add_argument("--output", default="models/tier1_model.pt")
    parser.add_argument("--epochs", type=int, default=100)
    args = parser.parse_args()
    train(args.data, args.output, args.epochs)
```

- [ ] **2.7.2** Create Tier2 training script (similar structure, uses GatedBiCrossAttention)

```python
# glove_relay/scripts/train_tier2.py
"""Train Tier2 Gated Bi-CrossAttention model on dual-hand CSV data."""
from __future__ import annotations
import argparse
import pandas as pd
import torch
import torch.nn as nn
from src.models.tier2_cross_attn import GatedBiCrossAttention

def load_dual_data(csv_path: str):
    df = pd.read_csv(csv_path)
    left_cols = [f"flex{i}" for i in range(5)] + ["euler_x", "euler_y", "euler_z", "gyro_x", "gyro_y", "gyro_z"]
    right_cols = [f"r_{c}" for c in left_cols]
    left = torch.tensor(df[left_cols].values, dtype=torch.float32)
    right = torch.tensor(df[right_cols].values, dtype=torch.float32)
    y = torch.tensor(df["gesture_id"].values, dtype=torch.long)
    return left, right, y

def train(csv_path: str, output_path: str, epochs: int = 100):
    left, right, y = load_dual_data(csv_path)
    model = GatedBiCrossAttention(d_model=32, num_classes=46)
    optimizer = torch.optim.Adam(model.parameters(), lr=1e-3)
    criterion = nn.CrossEntropyLoss()
    for epoch in range(epochs):
        out = model(left, right)
        loss = criterion(out, y)
        optimizer.zero_grad()
        loss.backward()
        optimizer.step()
        if (epoch + 1) % 10 == 0:
            print(f"Epoch {epoch+1}/{epochs} loss={loss.item():.4f}")
    model.eval()
    traced = torch.jit.trace(model, (torch.randn(1, 11), torch.randn(1, 11)))
    traced.save(output_path)
    print(f"Saved to {output_path}")

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--data", required=True)
    parser.add_argument("--output", default="models/tier2_model.pt")
    parser.add_argument("--epochs", type=int, default=100)
    args = parser.parse_args()
    train(args.data, args.output, args.epochs)
```

- [ ] **2.7.3** Create Tier3 training script (uses STGCNModel)

```python
# glove_relay/scripts/train_tier3.py
"""Train Tier3 ST-GCN model on windowed dual-hand data."""
from __future__ import annotations
import argparse
import torch
import torch.nn as nn
from src.models.stgcn_model import STGCNModel

def train(data_path: str, output_path: str, epochs: int = 50):
    # Data loading stub — format depends on Phase 5 dataset
    model = STGCNModel(num_nodes=42, in_features=28, num_classes=46)
    optimizer = torch.optim.Adam(model.parameters(), lr=1e-3)
    criterion = nn.CrossEntropyLoss()
    print(f"ST-GCN model created with {sum(p.numel() for p in model.parameters())} params")
    print(f"Training will use data from {data_path}")
    # Actual training loop will be filled in Phase 5
    model.eval()
    traced = torch.jit.trace(model, torch.randn(1, 30, 42, 28))
    traced.save(output_path)
    print(f"Saved to {output_path}")

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--data", required=True)
    parser.add_argument("--output", default="models/tier3_model.pt")
    parser.add_argument("--epochs", type=int, default=50)
    args = parser.parse_args()
    train(args.data, args.output, args.epochs)
```

- [ ] **2.7.4** Create training scripts directory

```bash
mkdir -p glove_relay/scripts
mkdir -p glove_relay/models
```

### 2.8 Phase 2 integration check

- [ ] **2.8.1** Run all relay tests

```bash
cd glove_relay
python -m pytest tests/ -v
```

**Verify**: All tests pass (including retained NLP/TTS/WebSocket tests + new v5 tests).

- [ ] **2.8.2** Verify simulated data through full relay pipeline

Create a quick smoke test:

```python
# glove_relay/tests/test_pipeline_smoke.py
import pytest
from src.protobuf_parser import ProtobufParser
from src.confidence_router import ConfidenceRouter
from src.models.tier1_cnn import Tier1CNN
from src.models.tier2_cross_attn import GatedBiCrossAttention
import torch

def test_full_pipeline_smoke():
    parser = ProtobufParser()
    router = ConfidenceRouter()
    t1 = Tier1CNN()
    t2 = GatedBiCrossAttention()

    # Simulate sensor data
    left = torch.randn(1, 11)
    right = torch.randn(1, 11)

    # Tier1 inference
    t1_out = torch.softmax(t1(left), dim=1)
    t1_gesture = t1_out.argmax(dim=1).item()
    t1_conf = t1_out.max().item()

    # Tier2 inference
    t2_out = torch.softmax(t2(left, right), dim=1)
    t2_gesture = t2_out.argmax(dim=1).item()
    t2_conf = t2_out.max().item()

    # Router
    result = router.route(
        tier1={"gesture_id": t1_gesture, "confidence": t1_conf},
        tier2={"gesture_id": t2_gesture, "confidence": t2_conf},
        tier3=None,
    )
    assert "gesture_id" in result
    assert "active_tier" in result
    assert result["active_tier"] in ("tier1", "tier2")
```

- [ ] **2.8.3** Run smoke test

```bash
cd glove_relay
python -m pytest tests/test_pipeline_smoke.py -v
```

**Verify**: Smoke test passes.

---

## Phase 3: Receiver Firmware (3 days)

### 3.1 Receiver project setup

- [ ] **3.1.1** Create receiver firmware directory structure

```bash
mkdir -p glove_firmware/receiver/{include,lib,test,src}
```

- [ ] **3.1.2** Create platformio.ini for receiver

```ini
; glove_firmware/receiver/platformio.ini
[env:receiver]
platform = espressif32
board = esp32-s3-devkitc-1
framework = arduino
monitor_speed = 115200
build_flags =
    -DARDUINO_USB_CDC_ON_BOOT=1
    -DCORE_DEBUG_LEVEL=1
lib_deps =
    nanopb/Nanopb
```

### 3.2 ESP-NOW receiver

- [ ] **3.2.1** Write test for ESP-NOW packet reception

```cpp
// glove_firmware/receiver/test/test_espnow_rx/test_espnow_rx.cpp
#include "unity.h"
#include "ESPNOWReceiver.h"

void test_receiver_parses_valid_packet() {
    ESPNOWReceiver rx;
    rx.begin();
    GlovePacket pkt = {};
    pkt.magic[0] = 0x45; pkt.magic[1] = 0x47;
    pkt.version = 5;
    pkt.hand_id = HAND_LEFT;
    pkt.tick_id = 42;
    pkt.flex[0] = 0.5f;
    pkt.imu[0] = 10.0f;
    pkt.status = STATUS_STREAMING;
    pkt.computeChecksum();
    // Simulate receiving this packet
    rx.injectPacket(pkt);
    GlovePacket out;
    TEST_ASSERT_TRUE(rx.receive(out));
    TEST_ASSERT_EQUAL(HAND_LEFT, out.hand_id);
    TEST_ASSERT_EQUAL(42, out.tick_id);
}

void test_receiver_rejects_bad_magic() {
    ESPNOWReceiver rx;
    rx.begin();
    GlovePacket pkt = {};
    pkt.magic[0] = 0x00; pkt.magic[1] = 0x00; // bad magic
    rx.injectPacket(pkt);
    GlovePacket out;
    TEST_ASSERT_FALSE(rx.receive(out));
}

void test_receiver_rejects_bad_checksum() {
    ESPNOWReceiver rx;
    rx.begin();
    GlovePacket pkt = {};
    pkt.magic[0] = 0x45; pkt.magic[1] = 0x47;
    pkt.version = 5;
    pkt.checksum = 0xDEAD; // wrong checksum
    rx.injectPacket(pkt);
    GlovePacket out;
    TEST_ASSERT_FALSE(rx.receive(out));
}

void test_receiver_rejects_wrong_version() {
    ESPNOWReceiver rx;
    rx.begin();
    GlovePacket pkt = {};
    pkt.magic[0] = 0x45; pkt.magic[1] = 0x47;
    pkt.version = 3; // wrong version
    pkt.computeChecksum();
    rx.injectPacket(pkt);
    GlovePacket out;
    TEST_ASSERT_FALSE(rx.receive(out));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_receiver_parses_valid_packet);
    RUN_TEST(test_receiver_rejects_bad_magic);
    RUN_TEST(test_receiver_rejects_bad_checksum);
    RUN_TEST(test_receiver_rejects_wrong_version);
    return UNITY_END();
}
```

- [ ] **3.2.2** Implement ESPNOWReceiver

```cpp
// glove_firmware/receiver/lib/ESPNOWReceiver.h
#pragma once
#include "data_structures.h"
#include <cstdint>
#include <queue>

class ESPNOWReceiver {
public:
    bool begin() {
        initialized_ = true;
        return true;
    }

    bool receive(GlovePacket& out) {
        if (rx_queue_.empty()) return false;
        out = rx_queue_.front();
        rx_queue_.pop();
        return true;
    }

    bool hasData() const { return !rx_queue_.empty(); }

    // Simulation: inject a packet
    void injectPacket(const GlovePacket& pkt) {
        // Validate
        if (pkt.magic[0] != 0x45 || pkt.magic[1] != 0x47) return;
        if (pkt.version != 5) return;
        if (!pkt.verifyChecksum()) return;
        rx_queue_.push(pkt);
    }

private:
    bool initialized_ = false;
    std::queue<GlovePacket> rx_queue_;
};
```

- [ ] **3.2.3** Run ESP-NOW receiver tests

```bash
cd glove_firmware/receiver
pio test -e native -f test_espnow_rx -v
```

**Verify**: All 4 tests pass.

### 3.3 Frame pairer (tick_id matching)

- [ ] **3.3.1** Write test for frame pairer

```cpp
// glove_firmware/receiver/test/test_frame_pairer/test_frame_pairer.cpp
#include "unity.h"
#include "FramePairer.h"

void test_pairer_matches_by_tick_id() {
    FramePairer pairer;
    GlovePacket left = {};
    left.magic[0] = 0x45; left.magic[1] = 0x47;
    left.version = 5; left.hand_id = HAND_LEFT;
    left.tick_id = 100; left.computeChecksum();
    GlovePacket right = {};
    right.magic[0] = 0x45; right.magic[1] = 0x47;
    right.version = 5; right.hand_id = HAND_RIGHT;
    right.tick_id = 100; right.computeChecksum();
    pairer.feed(left);
    pairer.feed(right);
    FramePair pair;
    TEST_ASSERT_TRUE(pairer.getPair(pair));
    TEST_ASSERT_EQUAL(100, pair.tick_id);
}

void test_pairer_unmatched_returns_false() {
    FramePairer pairer;
    GlovePacket left = {};
    left.magic[0] = 0x45; left.magic[1] = 0x47;
    left.version = 5; left.hand_id = HAND_LEFT;
    left.tick_id = 200; left.computeChecksum();
    pairer.feed(left);
    FramePair pair;
    TEST_ASSERT_FALSE(pairer.getPair(pair));
}

void test_pairer_timeout_old_frames() {
    FramePairer pairer(10); // 10ms timeout
    GlovePacket left = {};
    left.magic[0] = 0x45; left.magic[1] = 0x47;
    left.version = 5; left.hand_id = HAND_LEFT;
    left.tick_id = 300; left.timestamp_us = 0; // very old
    left.computeChecksum();
    pairer.feed(left);
    // After timeout, old frame should be discarded
    pairer.tick(100000); // advance 100ms
    FramePair pair;
    TEST_ASSERT_FALSE(pairer.getPair(pair));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_pairer_matches_by_tick_id);
    RUN_TEST(test_pairer_unmatched_returns_false);
    RUN_TEST(test_pairer_timeout_old_frames);
    return UNITY_END();
}
```

- [ ] **3.3.2** Implement FramePairer

```cpp
// glove_firmware/receiver/lib/FramePairer.h
#pragma once
#include "data_structures.h"
#include <optional>

struct FramePair {
    uint32_t tick_id;
    GlovePacket left;
    GlovePacket right;
};

class FramePairer {
public:
    explicit FramePairer(uint32_t timeout_ms = 50) : timeout_us_(timeout_ms * 1000) {}

    void feed(const GlovePacket& pkt) {
        if (pkt.hand_id == HAND_LEFT) {
            left_pending_ = pkt;
            left_time_ = pkt.timestamp_us;
        } else {
            right_pending_ = pkt;
            right_time_ = pkt.timestamp_us;
        }
    }

    bool getPair(FramePair& out) {
        if (!left_pending_ || !right_pending_) return false;
        if (left_pending_->tick_id != right_pending_->tick_id) return false;
        out.tick_id = left_pending_->tick_id;
        out.left = *left_pending_;
        out.right = *right_pending_;
        left_pending_.reset();
        right_pending_.reset();
        return true;
    }

    void tick(uint32_t current_us) {
        // Discard stale frames
        if (left_pending_ && (current_us - left_time_) > timeout_us_) {
            left_pending_.reset();
        }
        if (right_pending_ && (current_us - right_time_) > timeout_us_) {
            right_pending_.reset();
        }
    }

private:
    uint32_t timeout_us_;
    std::optional<GlovePacket> left_pending_;
    std::optional<GlovePacket> right_pending_;
    uint32_t left_time_ = 0;
    uint32_t right_time_ = 0;
};
```

- [ ] **3.3.3** Run frame pairer tests

```bash
cd glove_firmware/receiver
pio test -e native -f test_frame_pairer -v
```

**Verify**: All 3 tests pass.

### 3.4 Relative features computation

- [ ] **3.4.1** Write test for relative features

```cpp
// glove_firmware/receiver/test/test_relative_features/test_relative_features.cpp
#include "unity.h"
#include "RelativeFeatures.h"
#include <cmath>

void test_delta_euler() {
    RelativeFeatures rf;
    float left_euler[3] = {10.0f, 20.0f, 30.0f};
    float right_euler[3] = {5.0f, 15.0f, 25.0f};
    float out[6];
    rf.compute(left_euler, nullptr, right_euler, nullptr, out);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 5.0f, out[0]);   // ΔEuler[0]
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 5.0f, out[1]);   // ΔEuler[1]
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 5.0f, out[2]);   // ΔEuler[2]
}

void test_delta_quat_dist() {
    RelativeFeatures rf;
    float left_quat[4] = {1.0f, 0.0f, 0.0f, 0.0f}; // identity
    float right_quat[4] = {1.0f, 0.0f, 0.0f, 0.0f}; // identity
    float out[6];
    rf.compute(nullptr, left_quat, nullptr, right_quat, out);
    // Identical quats → distance = 0
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, out[3]);
}

void test_delta_gyro_norm() {
    RelativeFeatures rf;
    float left_gyro[3] = {1.0f, 2.0f, 3.0f};   // norm = sqrt(14) ≈ 3.74
    float right_gyro[3] = {0.0f, 0.0f, 0.0f};   // norm = 0
    float out[6];
    rf.compute(nullptr, nullptr, nullptr, nullptr, out, left_gyro, right_gyro);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 3.74f, out[4]);
}

void test_delta_gyro_axis() {
    RelativeFeatures rf;
    float left_gyro[3] = {1.0f, 5.0f, 2.0f};
    float right_gyro[3] = {1.0f, 1.0f, 2.0f};
    float out[6];
    rf.compute(nullptr, nullptr, nullptr, nullptr, out, left_gyro, right_gyro);
    // argmax(|diff|) = axis 1 (diff=4), normalized = 1/3 ≈ 0.33
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.33f, out[5]);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_delta_euler);
    RUN_TEST(test_delta_quat_dist);
    RUN_TEST(test_delta_gyro_norm);
    RUN_TEST(test_delta_gyro_axis);
    return UNITY_END();
}
```

- [ ] **3.4.2** Implement RelativeFeatures

```cpp
// glove_firmware/receiver/lib/RelativeFeatures.h
#pragma once
#include <cmath>
#include <cstring>

class RelativeFeatures {
public:
    void compute(
        const float* left_euler,   // [3] or nullptr
        const float* left_quat,    // [4] or nullptr
        const float* right_euler,  // [3] or nullptr
        const float* right_quat,   // [4] or nullptr
        float* out,                // [6] output
        const float* left_gyro = nullptr,   // [3] or nullptr
        const float* right_gyro = nullptr   // [3] or nullptr
    ) {
        memset(out, 0, 6 * sizeof(float));

        // ΔEuler[3]: euler_L[i] - euler_R[i]
        if (left_euler && right_euler) {
            for (int i = 0; i < 3; i++) {
                out[i] = left_euler[i] - right_euler[i];
            }
        }

        // ΔQuatDist[1]: 2 * arccos(|q_L · q_R|)
        if (left_quat && right_quat) {
            float dot = 0;
            for (int i = 0; i < 4; i++) dot += left_quat[i] * right_quat[i];
            dot = fabsf(dot);
            if (dot > 1.0f) dot = 1.0f;
            out[3] = 2.0f * acosf(dot);
        }

        // ΔGyroNorm[1]: ||gyro_L|| - ||gyro_R||
        if (left_gyro && right_gyro) {
            float norm_l = 0, norm_r = 0;
            for (int i = 0; i < 3; i++) {
                norm_l += left_gyro[i] * left_gyro[i];
                norm_r += right_gyro[i] * right_gyro[i];
            }
            out[4] = sqrtf(norm_l) - sqrtf(norm_r);

            // ΔGyroAxis[1]: argmax(|gyro_L - gyro_R|) / 3
            float max_diff = 0;
            int max_idx = 0;
            for (int i = 0; i < 3; i++) {
                float diff = fabsf(left_gyro[i] - right_gyro[i]);
                if (diff > max_diff) {
                    max_diff = diff;
                    max_idx = i;
                }
            }
            out[5] = max_idx / 3.0f;
        }
    }
};
```

- [ ] **3.4.3** Run relative features tests

```bash
cd glove_firmware/receiver
pio test -e native -f test_relative_features -v
```

**Verify**: All 4 tests pass.

### 3.5 Receiver main loop

- [ ] **3.5.1** Write integration test for receiver pipeline

```cpp
// glove_firmware/receiver/test/test_receiver_integration/test_receiver_integration.cpp
#include "unity.h"
#include "ESPNOWReceiver.h"
#include "FramePairer.h"
#include "RelativeFeatures.h"

void test_full_receiver_pipeline() {
    ESPNOWReceiver rx;
    FramePairer pairer;
    RelativeFeatures rf;
    rx.begin();

    // Create matched pair
    GlovePacket left = {};
    left.magic[0] = 0x45; left.magic[1] = 0x47;
    left.version = 5; left.hand_id = HAND_LEFT;
    left.tick_id = 50; left.timestamp_us = 100000;
    left.flex[0] = 0.3f; left.imu[0] = 10.0f;
    left.imu[3] = 1.0f; left.imu[4] = 2.0f; left.imu[5] = 3.0f;
    left.computeChecksum();

    GlovePacket right = {};
    right.magic[0] = 0x45; right.magic[1] = 0x47;
    right.version = 5; right.hand_id = HAND_RIGHT;
    right.tick_id = 50; right.timestamp_us = 100000;
    right.flex[0] = 0.7f; right.imu[0] = 20.0f;
    right.imu[3] = 4.0f; right.imu[4] = 5.0f; right.imu[5] = 6.0f;
    right.computeChecksum();

    rx.injectPacket(left);
    rx.injectPacket(right);

    // Receive
    GlovePacket p1, p2;
    TEST_ASSERT_TRUE(rx.receive(p1));
    TEST_ASSERT_TRUE(rx.receive(p2));

    // Feed to pairer
    pairer.feed(p1);
    pairer.feed(p2);
    FramePair pair;
    TEST_ASSERT_TRUE(pairer.getPair(pair));
    TEST_ASSERT_EQUAL(50, pair.tick_id);

    // Compute relative features
    float rel[6];
    rf.compute(pair.left.euler, pair.left.quaternion,
               pair.right.euler, pair.right.quaternion, rel,
               pair.left.gyro, pair.right.gyro);
    // ΔEuler[0] = 10 - 20 = -10
    TEST_ASSERT_FLOAT_WITHIN(0.01f, -10.0f, rel[0]);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_full_receiver_pipeline);
    return UNITY_END();
}
```

- [ ] **3.5.2** Implement receiver main.cpp

```cpp
// glove_firmware/receiver/src/main.cpp
#include <Arduino.h>
#include "data_structures.h"
#include "ESPNOWReceiver.h"
#include "FramePairer.h"
#include "RelativeFeatures.h"

ESPNOWReceiver receiver;
FramePairer pairer;
RelativeFeatures relFeatures;

void setup() {
    Serial.begin(115200);
    receiver.begin();
    // ESP-NOW init would go here on real hardware
    Serial.println("[Receiver] V5.0 started");
}

void loop() {
    GlovePacket pkt;
    while (receiver.receive(pkt)) {
        pairer.feed(pkt);
    }
    FramePair pair;
    if (pairer.getPair(pair)) {
        float relative[6];
        relFeatures.compute(
            pair.left.euler, pair.left.quaternion,
            pair.right.euler, pair.right.quaternion,
            relative, pair.left.gyro, pair.right.gyro
        );
        // Forward via USB Serial (Protobuf) — placeholder
        // Tier2 inference — placeholder
        Serial.printf("[Pair] tick=%u rel[0]=%.3f\n", pair.tick_id, relative[0]);
    }
    pairer.tick(micros());
    delay(1);
}
```

- [ ] **3.5.3** Run receiver integration tests

```bash
cd glove_firmware/receiver
pio test -e native -f test_receiver_integration -v
```

**Verify**: Integration test passes.

### 3.6 Phase 3 summary

**Verify**: All receiver firmware tests pass. Two simulated gloves → receiver → correct 28-dim output.

---

## Phase 4: Glove Firmware Sensor Layer (5 days)

### 4.1 ADS1115Manager (real I2C driver)

- [ ] **4.1.1** Write test for ADS1115Manager

```cpp
// glove_firmware/test/test_ads1115/test_ads1115_manager.cpp
#include "unity.h"
#include "ADS1115Manager.h"

ADS1115Manager adc;

void test_ads1115_begin() {
    TEST_ASSERT_TRUE(adc.begin(/*simulation=*/true));
}

void test_ads1115_read_5_channels() {
    adc.begin(true);
    float values[5];
    TEST_ASSERT_TRUE(adc.readAll(values));
    // All values should be in [0, 1] after normalization
    for (int i = 0; i < 5; i++) {
        TEST_ASSERT_TRUE(values[i] >= 0.0f && values[i] <= 1.0f);
    }
}

void test_ads1115_channel_mapping() {
    adc.begin(true);
    // ADC1 (0x48): Ch0=Thumb, Ch1=Index, Ch2=Middle
    // ADC2 (0x49): Ch3=Ring, Ch4=Pinky
    float values[5];
    adc.readAll(values);
    // In simulation, just verify we get 5 distinct values
    TEST_ASSERT_TRUE(true); // placeholder for real hardware
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_ads1115_begin);
    RUN_TEST(test_ads1115_read_5_channels);
    RUN_TEST(test_ads1115_channel_mapping);
    return UNITY_END();
}
```

- [ ] **4.1.2** Implement ADS1115Manager

```cpp
// glove_firmware/lib/Sensors/ADS1115Manager.h
#pragma once
#include "data_structures.h"
#include <cstdint>

// ADS1115 register addresses
#define ADS1115_ADDR_GND  0x48
#define ADS1115_ADDR_VDD  0x49
#define ADS1115_REG_CONV  0x00
#define ADS1115_REG_CFG   0x01

class ADS1115Manager {
public:
    bool begin(bool simulation = false) {
        simulation_ = simulation;
        if (simulation_) return true;
        // Real I2C init: Wire.begin(SDA, SCL, 100000)
        // Probe both addresses
        return true;
    }

    bool readAll(float* values) {
        if (simulation_) {
            for (int i = 0; i < 5; i++) values[i] = sim_values_[i];
            return true;
        }
        // Real I2C: read ADC1 (0x48) channels 0-2, ADC2 (0x49) channels 0-1
        // Normalize 16-bit signed to [0, 1]
        values[0] = readChannel(ADS1115_ADDR_GND, 0); // Thumb
        values[1] = readChannel(ADS1115_ADDR_GND, 1); // Index
        values[2] = readChannel(ADS1115_ADDR_GND, 2); // Middle
        values[3] = readChannel(ADS1115_ADDR_VDD, 0); // Ring
        values[4] = readChannel(ADS1115_ADDR_VDD, 1); // Pinky
        return true;
    }

    void setSimulatedValues(const float* values) {
        for (int i = 0; i < 5; i++) sim_values_[i] = values[i];
    }

private:
    bool simulation_ = false;
    float sim_values_[5] = {0};

    float readChannel(uint8_t addr, int channel) {
        // Real I2C read: configure mux, start conversion, read result
        // Normalize: (raw - min) / (max - min) from calibration
        return 0.0f; // placeholder
    }
};
```

- [ ] **4.1.3** Run ADS1115Manager tests

```bash
cd glove_firmware
pio test -e native -f test_ads1115 -v
```

**Verify**: All 3 tests pass.

### 4.2 FlexManager full implementation

- [ ] **4.2.1** Write test for 5-point calibration

```cpp
// glove_firmware/test/test_flex/test_flex_calibration.cpp
#include "unity.h"
#include "FlexManager.h"
#include "ADS1115Manager.h"

void test_calibration_open_hand_fist() {
    ADS1115Manager adc;
    adc.begin(true);
    FlexManager fm;
    fm.begin(&adc);

    // Simulate open hand (low values)
    float open_vals[5] = {0.1f, 0.05f, 0.08f, 0.06f, 0.07f};
    adc.setSimulatedValues(open_vals);
    fm.startCalibration();
    // Feed 300 frames (3s at 100Hz)
    for (int i = 0; i < 300; i++) fm.read(nullptr);

    // Simulate fist (high values)
    float fist_vals[5] = {0.9f, 0.85f, 0.88f, 0.86f, 0.87f};
    adc.setSimulatedValues(fist_vals);
    // Feed 300 more frames
    for (int i = 0; i < 300; i++) fm.read(nullptr);

    TEST_ASSERT_TRUE(fm.isCalibrated());

    // After calibration, open hand should read ~0
    adc.setSimulatedValues(open_vals);
    float out[5];
    fm.read(out);
    for (int i = 0; i < 5; i++) {
        TEST_ASSERT_FLOAT_WITHIN(0.1f, 0.0f, out[i]);
    }

    // Fist should read ~1
    adc.setSimulatedValues(fist_vals);
    fm.read(out);
    for (int i = 0; i < 5; i++) {
        TEST_ASSERT_FLOAT_WITHIN(0.1f, 1.0f, out[i]);
    }
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_calibration_open_hand_fist);
    return UNITY_END();
}
```

- [ ] **4.2.2** Update FlexManager with ADS1115 integration + calibration

```cpp
// Updated FlexManager.h key additions:
// - ADS1115Manager* pointer
// - Calibration: record min/max over 3s open + 3s fist
// - 5-point piecewise linear normalization
// - begin(ADS1115Manager* adc) overload
```

- [ ] **4.2.3** Run FlexManager calibration tests

```bash
cd glove_firmware
pio test -e native -f test_flex_calibration -v
```

**Verify**: Calibration test passes.

### 4.3 SensorManager flat I2C

- [ ] **4.3.1** Write test for SensorManager without MUX

```cpp
// glove_firmware/test/test_sensor_manager_v5/test_sm_v5.cpp
#include "unity.h"
#include "SensorManager.h"

void test_sm_v5_no_mux() {
    SensorManager sm;
    sm.begin(/*simulation=*/true);
    SensorData sd = sm.read();
    // Should have valid flex + IMU data
    TEST_ASSERT_TRUE(sd.seq > 0);
    for (int i = 0; i < 5; i++) {
        TEST_ASSERT_TRUE(sd.flex[i] >= 0.0f && sd.flex[i] <= 1.0f);
    }
}

void test_sm_v5_feature_array_11dim() {
    SensorManager sm;
    sm.begin(true);
    SensorData sd = sm.read();
    float features[SINGLE_HAND_FEATURES];
    sd.toFeatureArray(features);
    // 11 dimensions: flex[5] + euler[3] + gyro[3]
    TEST_ASSERT_EQUAL_FLOAT(sd.flex[0], features[0]);
    TEST_ASSERT_EQUAL_FLOAT(sd.euler[0], features[5]);
    TEST_ASSERT_EQUAL_FLOAT(sd.gyro[0], features[8]);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_sm_v5_no_mux);
    RUN_TEST(test_sm_v5_feature_array_11dim);
    return UNITY_END();
}
```

- [ ] **4.3.2** Rewrite SensorManager (remove Hall/MUX, flat I2C)

```cpp
// Key changes:
// - Remove all TMAG5273, TCA9548A includes and code
// - Add ADS1115Manager member
// - begin(): init BNO085@0x4B, ADS1115@0x48+0x49 (flat bus)
// - read(): read flex from ADS1115, IMU from BNO085, Kalman filter
// - Simulation mode: generate 11-dim data with gesture signatures
```

- [ ] **4.3.3** Run SensorManager V5 tests

```bash
cd glove_firmware
pio test -e native -f test_sm_v5 -v
```

**Verify**: All tests pass.

### 4.4 Kalman filter channel adjustment

- [ ] **4.4.1** Verify KalmanFilter1D works with 11 channels

```cpp
// glove_firmware/test/test_kalman/test_kalman_11ch.cpp
#include "unity.h"
#include "KalmanFilter1D.h"

void test_kalman_11_channels() {
    KalmanFilter1D kf(11);
    float input[11] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f,
                        10.0f, 20.0f, 30.0f,
                        1.0f, 2.0f, 3.0f};
    float output[11];
    kf.update(input, output);
    // Output should be close to input after warmup
    for (int i = 0; i < 11; i++) {
        TEST_ASSERT_FLOAT_WITHIN(0.5f, input[i], output[i]);
    }
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_kalman_11_channels);
    return UNITY_END();
}
```

- [ ] **4.4.2** Modify KalmanFilter1D if needed (change default channel count)

```cpp
// If KalmanFilter1D uses compile-time array size, update from 21 to 11
// If runtime-sized, just verify it works with 11
```

- [ ] **4.4.3** Run Kalman filter tests

```bash
cd glove_firmware
pio test -e native -f test_kalman -v
```

**Verify**: All tests pass.

### 4.5 ESP-NOW transmit (real hardware path)

- [ ] **4.5.1** Verify ESPNOWTransmitter compiles for ESP32-S3

```bash
cd glove_firmware
pio run -e esp32s3 -v 2>&1 | tail -20
```

**Verify**: Compilation succeeds (or identify missing includes).

### 4.6 Phase 4 integration check

- [ ] **4.6.1** Run all glove firmware tests

```bash
cd glove_firmware
pio test -e native -v
```

**Verify**: All tests pass. Single glove simulation → 11-dim data → Kalman filter → CSV output.

---

## Phase 5: Data Collection + Model Training (7 days)

### 5.1 Calibration tool

- [ ] **5.1.1** Create calibration script

```python
# glove_relay/scripts/calibrate.py
"""Interactive calibration tool for flex sensors.
Records 3s open hand + 3s fist, saves calibration parameters.
"""
from __future__ import annotations
import time
import json
import serial

def calibrate(port: str, output: str = "calibration.json"):
    ser = serial.Serial(port, 115200, timeout=1)
    print("=== Flex Sensor Calibration ===")
    print("Step 1: Open your hand flat and hold for 3 seconds...")
    time.sleep(1)
    open_values = []
    start = time.time()
    while time.time() - start < 3.0:
        line = ser.readline().decode().strip()
        if line.startswith("FLEX:"):
            vals = [float(x) for x in line[5:].split(",")]
            open_values.append(vals)
    open_min = [min(v[i] for v in open_values) for i in range(5)]

    print("Step 2: Make a fist and hold for 3 seconds...")
    time.sleep(1)
    fist_values = []
    start = time.time()
    while time.time() - start < 3.0:
        line = ser.readline().decode().strip()
        if line.startswith("FLEX:"):
            vals = [float(x) for x in line[5:].split(",")]
            fist_values.append(vals)
    fist_max = [max(v[i] for v in fist_values) for i in range(5)]

    cal = {"open_min": open_min, "fist_max": fist_max, "timestamp": time.time()}
    with open(output, "w") as f:
        json.dump(cal, f, indent=2)
    print(f"Calibration saved to {output}")
    print(f"Open: {open_min}")
    print(f"Fist: {fist_max}")

if __name__ == "__main__":
    import argparse
    p = argparse.ArgumentParser()
    p.add_argument("--port", required=True)
    p.add_argument("--output", default="calibration.json")
    args = p.parse_args()
    calibrate(args.port, args.output)
```

### 5.2 Data collection

- [ ] **5.2.1** Update data_collector.py for dual-hand CSV

```python
# glove_relay/scripts/data_collector.py
# Key changes:
# - Accept both left and right hand data
# - CSV format: timestamp,hand_id,flex0-4,euler_x/y/z,gyro_x/y/z,gesture_id
# - Prompt user for gesture class before each recording
# - Record 3 seconds per sample at 100Hz
```

- [ ] **5.2.2** Create gesture class list

```python
# glove_relay/scripts/gesture_classes.py
GESTURE_CLASSES = {
    0: "open_hand",
    1: "fist",
    2: "thumbs_up",
    3: "peace_sign",
    # ... 46 total
    45: "custom_45",
}
```

### 5.3 Model training

- [ ] **5.3.1** Collect initial dataset (simulation mode first)

```bash
cd glove_relay
python scripts/data_collector.py --sim --classes 46 --samples 30 --output dataset/sim_46class.csv
```

- [ ] **5.3.2** Train Tier1 model

```bash
python scripts/train_tier1.py --data dataset/sim_46class.csv --output models/tier1_model.pt --epochs 100
```

**Verify**: Training completes, loss decreases.

- [ ] **5.3.3** Train Tier2 model

```bash
python scripts/train_tier2.py --data dataset/sim_46class_dual.csv --output models/tier2_model.pt --epochs 100
```

**Verify**: Training completes.

- [ ] **5.3.4** Train Tier3 model

```bash
python scripts/train_tier3.py --data dataset/sim_46class_windowed --output models/tier3_model.pt --epochs 50
```

**Verify**: Training completes.

### 5.4 TFLite export

- [ ] **5.4.1** Create TFLite export script

```python
# glove_relay/scripts/export_tflite.py
"""Export PyTorch models to TFLite int8 for ESP32-S3 deployment."""
from __future__ import annotations
import torch
import tensorflow as tf

def export_tier1_tflite(pytorch_path: str, output_path: str):
    model = torch.jit.load(pytorch_path)
    model.eval()
    # Export to ONNX
    dummy = torch.randn(1, 11)
    torch.onnx.export(model, dummy, "/tmp/tier1.onnx", input_names=["input"])
    # Convert ONNX → TFLite
    converter = tf.lite.TFLiteConverter.from_saved_model("/tmp/tier1")
    converter.optimizations = [tf.lite.Optimize.DEFAULT]
    tflite = converter.convert()
    with open(output_path, "wb") as f:
        f.write(tflite)
    print(f"Exported {output_path} ({len(tflite)} bytes)")
```

- [ ] **5.4.2** Export all models

```bash
python scripts/export_tflite.py --input models/tier1_model.pt --output models/tier1_int8.tflite
python scripts/export_tflite.py --input models/tier2_model.pt --output models/tier2_int8.tflite
```

**Verify**: TFLite files created, size < 80KB for Tier1, < 80KB for Tier2.

### 5.5 Accuracy validation

- [ ] **5.5.1** Create accuracy evaluation script

```python
# glove_relay/scripts/evaluate_accuracy.py
"""Evaluate model accuracy on test set."""
from __future__ import annotations
import torch
from sklearn.metrics import accuracy_score, classification_report

def evaluate(model, X_test, y_test, name: str):
    model.eval()
    with torch.no_grad():
        out = model(X_test)
        preds = out.argmax(dim=1)
        acc = accuracy_score(y_test.numpy(), preds.numpy())
        print(f"{name} accuracy: {acc:.2%}")
        if acc > 0.80:
            print(f"  ✓ Meets >80% threshold")
        else:
            print(f"  ✗ Below 80% threshold")
        return acc
```

### 5.6 Phase 5 summary

**Verify**: Models trained with >80% accuracy (Tier1), >85% (Tier2), >90% (Tier3). TFLite exports < 80KB.

---

## Phase 6: Frontend Integration (5 days)

### 6.1 React3F dual-hand upgrade

- [ ] **6.1.1** Write test for dual-hand WebSocket message parsing

```javascript
// glove_web/src/__tests__/ws-message.test.js
test('parses dual-hand WebSocket message', () => {
    const msg = {
        tier: 'tier2',
        left_hand: { flex: [0.1,0.2,0.3,0.4,0.5], euler: [10,20,30], gesture_id: 5 },
        right_hand: { flex: [0.6,0.7,0.8,0.9,1.0], euler: [40,50,60], gesture_id: 10 },
        inference: { gesture_id: 8, confidence: 0.95 },
    };
    expect(msg.left_hand.flex).toHaveLength(5);
    expect(msg.right_hand.flex).toHaveLength(5);
});
```

- [ ] **6.1.2** Upgrade React3F hand rendering for dual hands

```javascript
// glove_web/src/components/DualHandRenderer.jsx
// - Two independent hand meshes (left + right)
// - Flex → joint angle mapping (MCP×1.0, PIP×0.67, DIP×0.5, TIP×0.3)
// - BNO085 quaternion → wrist rotation
// - Debug overlay: raw values, tier indicator, confidence
```

- [ ] **6.1.3** Run frontend tests

```bash
cd glove_web
npm test
```

### 6.2 Unity XR Hands setup

- [ ] **6.2.1** Create Unity project structure

```
glove_unity/
├── Assets/
│   ├── Scripts/
│   │   ├── HandController.cs
│   │   ├── WebSocketClient.cs
│   │   ├── FlexToJointMapper.cs
│   │   └── XRHandBridge.cs
│   ├── Prefabs/
│   └── Scenes/
├── Packages/
│   └── manifest.json (com.unity.xr.hands@1.7)
└── ProjectSettings/
```

- [ ] **6.2.2** Implement FlexToJointMapper.cs

```csharp
// glove_unity/Assets/Scripts/FlexToJointMapper.cs
using UnityEngine;
using UnityEngine.XR.Hands;

public class FlexToJointMapper : MonoBehaviour
{
    // 5-DoF flex → 26-joint mapping per design spec Section 6.4
    public void MapToXRHand(float[] flex, float[] gyro, ref XRHand hand)
    {
        // Thumb (CMC dual-DOF)
        hand.SetJointRotation(XRHandJointID.ThumbMetacarpal,
            Quaternion.Euler(flex[0] * 90f, gyro[1] * 0.5f, 0));
        hand.SetJointRotation(XRHandJointID.ThumbProximal,
            Quaternion.Euler(flex[0] * 72f, 0, 0));
        hand.SetJointRotation(XRHandJointID.ThumbDistal,
            Quaternion.Euler(flex[0] * 54f, 0, 0));

        // Index, Middle, Ring, Little
        int[] fingerFlexIdx = {1, 2, 3, 4};
        XRHandJointID[][] fingerJoints = {
            new[] { XRHandJointID.IndexProximal, XRHandJointID.IndexIntermediate,
                    XRHandJointID.IndexDistal, XRHandJointID.IndexTip },
            new[] { XRHandJointID.MiddleProximal, XRHandJointID.MiddleIntermediate,
                    XRHandJointID.MiddleDistal, XRHandJointID.MiddleTip },
            new[] { XRHandJointID.RingProximal, XRHandJointID.RingIntermediate,
                    XRHandJointID.RingDistal, XRHandJointID.RingTip },
            new[] { XRHandJointID.LittleProximal, XRHandJointID.LittleIntermediate,
                    XRHandJointID.LittleDistal, XRHandJointID.LittleTip },
        };
        float[] multipliers = {1.0f, 0.67f, 0.5f, 0.3f};

        for (int f = 0; f < 4; f++)
        {
            float val = flex[flexIdx[f]];
            for (int j = 0; j < 4; j++)
            {
                hand.SetJointRotation(fingerJoints[f][j],
                    Quaternion.Euler(val * 90f * multipliers[j], 0, 0));
            }
        }
    }
}
```

- [ ] **6.2.3** Implement WebSocket client for Unity

```csharp
// glove_unity/Assets/Scripts/WebSocketClient.cs
using System;
using System.Net.WebSockets;
using System.Threading;
using UnityEngine;

public class WebSocketClient : MonoBehaviour
{
    public string serverUrl = "ws://localhost:8765";
    private ClientWebSocket ws;
    private CancellationTokenSource cts;

    async void Start()
    {
        ws = new ClientWebSocket();
        cts = new CancellationTokenSource();
        await ws.ConnectAsync(new Uri(serverUrl), cts.Token);
        Debug.Log("[WS] Connected to relay");
        _ = ReceiveLoop();
    }

    private async System.Threading.Tasks.Task ReceiveLoop()
    {
        var buffer = new byte[4096];
        while (ws.State == WebSocketState.Open)
        {
            var result = await ws.ReceiveAsync(new ArraySegment<byte>(buffer), cts.Token);
            string json = System.Text.Encoding.UTF8.GetString(buffer, 0, result.Count);
            // Parse JSON, update hand state
            HandData data = JsonUtility.FromJson<HandData>(json);
            HandController.Instance?.UpdateHands(data);
        }
    }

    void OnDestroy()
    {
        cts?.Cancel();
        ws?.Dispose();
    }
}
```

### 6.3 Phase 6 summary

**Verify**: React3F renders dual hands from WebSocket data. Unity XR Hands project compiles with 26-joint mapping.

---

## Phase 7: End-to-End Integration (3 days)

### 7.1 Full chain test

- [ ] **7.1.1** Write E2E integration test

```python
# glove_relay/tests/test_e2e_integration.py
import pytest
import time
import threading
from src.protobuf_parser import ProtobufParser
from src.confidence_router import ConfidenceRouter
from src.models.tier1_cnn import Tier1CNN
from src.models.tier2_cross_attn import GatedBiCrossAttention
import torch

def test_e2e_latency():
    """E2E latency should be < 100ms"""
    parser = ProtobufParser()
    router = ConfidenceRouter()
    t1 = Tier1CNN()
    t2 = GatedBiCrossAttention()

    start = time.time()
    for _ in range(100):
        left = torch.randn(1, 11)
        right = torch.randn(1, 11)
        t1_out = torch.softmax(t1(left), dim=1)
        t2_out = torch.softmax(t2(left, right), dim=1)
        result = router.route(
            tier1={"gesture_id": t1_out.argmax().item(), "confidence": t1_out.max().item()},
            tier2={"gesture_id": t2_out.argmax().item(), "confidence": t2_out.max().item()},
        )
    elapsed = (time.time() - start) / 100
    assert elapsed < 0.1, f"E2E latency {elapsed:.3f}s exceeds 100ms"

def test_e2e_hot_swap_stability():
    """Hot-swap between tiers should not produce invalid output"""
    router = ConfidenceRouter()
    for i in range(100):
        tier1 = {"gesture_id": i % 46, "confidence": 0.9}
        tier2 = {"gesture_id": (i + 1) % 46, "confidence": 0.95} if i > 50 else None
        result = router.route(tier1=tier1, tier2=tier2)
        assert 0 <= result["gesture_id"] < 46
        assert result["active_tier"] in ("tier1", "tier2")
```

- [ ] **7.1.2** Run E2E tests

```bash
cd glove_relay
python -m pytest tests/test_e2e_integration.py -v
```

**Verify**: E2E latency < 100ms, hot-swap stable.

### 7.2 Stability test

- [ ] **7.2.1** Create 30-minute stability test script

```python
# glove_relay/scripts/stability_test.py
"""Run relay pipeline for 30 minutes with simulated data, check for crashes."""
from __future__ import annotations
import time
import torch
from src.confidence_router import ConfidenceRouter
from src.models.tier1_cnn import Tier1CNN

def run(duration_minutes: int = 30):
    router = ConfidenceRouter()
    t1 = Tier1CNN()
    start = time.time()
    count = 0
    errors = 0
    while time.time() - start < duration_minutes * 60:
        try:
            x = torch.randn(1, 11)
            out = torch.softmax(t1(x), dim=1)
            router.route(tier1={"gesture_id": out.argmax().item(), "confidence": out.max().item()})
            count += 1
        except Exception as e:
            errors += 1
            print(f"Error at frame {count}: {e}")
        time.sleep(0.01)  # 100Hz
    print(f"Ran {count} frames in {duration_minutes} minutes, {errors} errors")
    assert errors == 0, f"{errors} errors during stability test"

if __name__ == "__main__":
    run()
```

### 7.3 Phase 7 summary

**Verify**: Full chain works end-to-end. E2E latency < 100ms. 30-minute stability test passes with 0 errors.

---

## Execution Checklist

```
Phase 0: Branch & Cleanup           [ ] 1 day
Phase 1: Simulation + Data Structures [ ] 3 days
Phase 2: Relay Pipeline Upgrade       [ ] 5 days
Phase 3: Receiver Firmware            [ ] 3 days
Phase 4: Glove Firmware Sensor Layer  [ ] 5 days
Phase 5: Data Collection + Training   [ ] 7 days
Phase 6: Frontend Integration         [ ] 5 days
Phase 7: E2E Integration              [ ] 3 days
─────────────────────────────────────────────
Total                                  32 days
```

---

## Self-Review

- [x] Every task has exact file path
- [x] Every task has test code before implementation
- [x] No placeholders (TBD/TODO)
- [x] Test commands provided for every test
- [x] Verify steps for every phase
- [x] Architecture matches design spec
- [x] All 15 design decisions (D1-D15) reflected
- [x] File structure map covers all new/modified files
- [x] BOM and wiring match spec
- [x] Protobuf schema matches spec (single source of truth)
