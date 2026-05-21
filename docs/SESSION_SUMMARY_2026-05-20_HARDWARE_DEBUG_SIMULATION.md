# Session Summary - Hardware Debug & Simulation Mode Implementation

**Date**: 2026-05-20  
**Project**: EdgeAI Data Glove V3  
**Phase**: P3 - L1 Edge Inference (Edge Impulse MVP Path A)  
**Session Focus**: Hardware debugging, I2C communication troubleshooting, simulation mode implementation

---

## Project Status Overview

### Current Phase
**Phase 3 (Active)**: L1 Edge Inference using Edge Impulse MVP approach
- Path A: `edge-impulse-data-forwarder` with serial CSV output → train 1D-CNN in Edge Impulse → export Arduino library → integrate into firmware
- Target: <3ms inference latency, 20 gesture classes (simplified), >90% Top-1 accuracy

### Hardware Readiness
- **ESP32-S3-DevKitC-1 N16R8**: Connected via USB CDC (port: `/dev/ttyACM0`)
- **PCA9548A I2C Multiplexer**: Wired but not responding on I2C (address 0x70)
- **BNO085 IMU**: Wired to mux Channel 5 but not responding
- **TMAG5273 Hall Sensors**: Not yet arrived (5 sensors planned for Channels 0-4)
- **Status**: Firmware operational, hardware debugging in progress

---

## Hardware Configuration

### Wiring Details

**Main I2C Bus** (ESP32-S3 → PCA9548A):
```
ESP32-S3                PCA9548A               Notes
────────                ──────────             ─────
GPIO8 (SDA)  ──────────→  SDA          Main bus data line
GPIO9 (SCL)  ──────────→  SCL          Main bus clock line
3.3V ──[2kΩ]── GPIO8                      Pull-up resistor (SDA)
3.3V ──[2kΩ]── GPIO9                      Pull-up resistor (SCL)
3.3V         ──────────→  VCC           Power supply
GND          ──────────→  GND           Ground
/RST         ──────────→  3.3V          Reset pin pulled high
```

**Multiplexer Channel Assignments**:
- **Channel 0-4**: TMAG5273 Hall sensors (SD0-SD4/SC0-SC4) — **NOT YET CONNECTED**
- **Channel 5**: BNO085 IMU (SD5/SC5) — Connected
- **Channel 6-7**: Reserved/unused

**BNO085 IMU Connection** (Channel 5):
```
PCA9548A CH5            BNO085                 Notes
───────────             ───────                ─────
SD5         ──────────→  SDA           Channel 5 downstream bus
SC5         ──────────→  SCL           Channel 5 downstream clock
3.3V        ──────────→  VCC           Power (via main bus)
GND         ──────────→  GND           Ground
GPIO21      ──────────→  INT           Interrupt pin (optional)
PS0         ──────────→  GND           Address bit 0 = 0
PS1         ──────────→  GND           Address bit 1 = 0
                                    → I2C Address: 0x4A
```

### Pull-Up Resistor Configuration

**Main Bus**: 2kΩ pull-ups (SDA/SCL) — **Installed**  
**Sub-Channels**: 4.7kΩ pull-ups — **Not installed** (will be added when TMAG5273 sensors arrive)

**SOP Specification** (docs/HARDWARE_ASSEMBLY_GUIDE.md):  
- Main bus: 2kΩ recommended (user used 2kΩ — acceptable but slower rise time)
- Sub-channels: 4.7kΩ per downstream sensor bus

---

## Key Decisions Made

### 1. Simulation Mode Implementation

**Decision**: Implement graceful degradation when PCA9548A is not detected  
**Reasoning**:
- Hardware debugging takes time (physical wiring verification, multimeter measurements)
- Firmware development should not be blocked by hardware issues
- Edge Impulse data collection can proceed with synthetic data while hardware is being debugged
- Allows parallel development tracks: firmware + hardware assembly

**Implementation**:
- Modified `SensorManager.h` to detect PCA9548A init failure
- Fall back to synthetic data generation for 20 gesture classes
- Auto-cycle gestures every 3 seconds (300 frames @ 100Hz sampling)
- Kalman filtering still applied to synthetic data (validates signal processing pipeline)
- CSV output format unchanged (compatible with Edge Impulse data forwarder)

### 2. Gesture Class Selection

**Decision**: 20 gesture classes (simplified)  
**Options**: 5 classes (minimal), 20 classes (simplified), 46 classes (full)  
**Reasoning**:
- 5 classes too limited for meaningful classification
- 46 classes requires complex dataset and longer training time
- 20 classes provides good coverage for initial proof-of-concept
- Can expand to 46 classes after hardware is fully functional

### 3. Hardware Debugging Approach

**Decision**: Provide detailed DM40B multimeter wiring verification guide  
**Reasoning**:
- User has multimeter but may not be familiar with I2C bus testing procedures
- Systematic verification prevents guessing and reduces trial-and-error time
- Written guide allows user to debug independently without Claude assistance
- Safety reminders prevent multimeter damage (measuring resistance on powered circuits)

---

## Problems Encountered

### 1. Permission Denied on `/dev/ttyACM0`

**Error**: `PermissionError: [Errno 13] Attempted access to '/dev/ttyACM0'`

**Root Cause**: User not in `dialout` group, device permissions restricted

**Solution**:
1. User provided sudo password (single space character)
2. Installed udev rules: `/etc/udev/rules.d/99-platformio-udev.rules`
3. Added user to `dialout` group (already member, session needed refresh)
4. Permanent fix: udev rules set `MODE="0666"` for ttyACM devices

**Code Reference**: See `/tmp/99-platformio-udev.rules` (temporary copy created during session)

### 2. USB CDC Serial Not Outputting After Boot

**Symptom**: ESP32-S3 bootloader messages visible but Arduino application silent after reset

**Root Cause**: ESP32-S3 USB CDC requires explicit initialization flag

**Solution**: Added `-DARDUINO_USB_CDC_ON_BOOT=1` to `platformio.ini` build_flags

**File Modified**: `glove_firmware/platformio.ini`  
**Line**: `-DARDUINO_USB_CDC_ON_BOOT=1`

### 3. Core Dump Checksum Error Blocking Boot

**Error**: Previous firmware upload left corrupted flash, blocking new upload

**Solution**: Erased entire flash with `pio run -t erase` before re-upload

**Command**: `pio run -t erase` (full flash erase)

### 4. I2C Scanner Hanging During Address Scan

**Symptom**: ESP-IDF I2C driver blocking when scanning addresses, scanner hangs

**Root Cause**: ESP-IDF I2C implementation has blocking behavior on NACK responses

**Solution**: Reduced scan to minimal address tests (0x70, 0x4A, 0x22) instead of full range

**Code Location**: `lib/Sensors/PCA9548A.h` (I2C scanner implementation)

### 5. Build Error: Braces Around Scalar Initializer

**Error**: `error: braces around scalar initializer for type 'float'`

**Root Cause**: `PROGMEM` + triple brace syntax incompatible with struct containing float array  
**Code**:
```cpp
// WRONG
static const GestureSignature gestures[20] PROGMEM = {
    {{0,0,0.1f, 0,0,0.1f, ...}, 0,0,0, 0,0,0},  // Triple braces + PROGMEM
};

// CORRECT
static const GestureSignature gestures[20] = {
    {{0,0,0.1f, 0,0,0.1f, ...}, 0,0,0, 0,0,0},  // Double braces, no PROGMEM
};
```

**Solution**: Removed `PROGMEM` qualifier, used double brace syntax for struct initialization

**File Modified**: `lib/Sensors/SensorManager.h` (lines 331-372)

### 6. I2C Devices Not Responding (PCA9548A, BNO085)

**Symptom**: `I2C scan: err=5 (NACK)` for addresses 0x70 (PCA9548A) and 0x4A (BNO085)

**Status**: **UNRESOLVED** — hardware wiring verification needed

**Suspected Causes**:
- Devices may not be powered (no LEDs visible on modules)
- Wiring not connected properly (jumper wires may be loose)
- Pull-up resistors incorrect (2kΩ instead of recommended 2kΩ — may cause slow rise time)

**Next Action**: Follow DM40B multimeter debugging guide (see below)

---

## Solutions Implemented

### 1. Udev Rules for Serial Port Access

**File**: `/etc/udev/rules.d/99-platformio-udev.rules`  
**Content**: Permanent serial port access for ESP32-S3 and common dev boards

```bash
# ESP32-S3 DevKit (USB CDC ACM)
SUBSYSTEM=="tty", ATTRS{idVendor}=="303a", ATTRS{idProduct}=="1001", MODE="0666"
SUBSYSTEM=="tty", ATTRS{idVendor}=="303a", ATTRS{idProduct}=="000?", MODE="0666"

# Generic USB CDC ACM
KERNEL=="ttyACM[0-9]*", MODE="0666"
KERNEL=="ttyUSB[0-9]*", MODE="0666"
```

**Verification**: `ls -l /dev/ttyACM0` should show `crw-rw-rw-` (mode 0666)

### 2. PlatformIO Build Flags for USB CDC

**File**: `glove_firmware/platformio.ini`  
**Modification**: Added USB CDC initialization flag

```ini
build_flags =
    -DBOARD_HAS_PSRAM
    -DCORE_DEBUG_LEVEL=3
    -DARDUINO_USB_CDC_ON_BOOT=1    # ← Added for ESP32-S3 USB CDC Serial
    -DCONFIG_ASYNC_TCP_USE_WDT=0
    ...
```

**Result**: Serial output available immediately after boot (no delay)

### 3. SensorManager Simulation Mode

**File**: `lib/Sensors/SensorManager.h`  
**Key Modifications**:

#### Added Simulation Mode State
```cpp
private:
    bool      _simulation_mode;
    uint8_t   _sim_gesture_id;
    uint32_t  _sim_frame_counter;
    uint32_t  _sim_transition_timer;
```

#### Graceful Degradation in begin()
```cpp
bool begin() {
    // ... I2C initialization ...
    
    if (!_mux.begin()) {
        Serial.println("[SensorManager] WARNING: PCA9548A not found!");
        Serial.println("[SensorManager] Entering SIMULATION mode");
        Serial.println("[SensorManager] 20 gesture classes, 3s cycle each");
        _simulation_mode = true;
        _initialized = true;
        _imu_ok = false;
        return true;  // Still return true — simulation is valid
    }
    
    // ... normal initialization ...
}
```

#### Synthetic Gesture Data Generation
```cpp
struct GestureSignature {
    float hall[15];           // 5 sensors × 3 axes
    float roll, pitch, yaw;   // Euler angles (degrees)
    float gx, gy, gz;         // Gyroscope (deg/s)
};

static const GestureSignature gestures[20] = {
    // 0: open hand
    {{0,0,0.1f, 0,0,0.1f, 0,0,0.1f, 0,0,0.1f, 0,0,0.1f}, 0,0,0, 0,0,0},
    // 1: fist
    {{0,0,0.9f, 0,0,0.9f, 0,0,0.9f, 0,0,0.9f, 0,0,0.9f}, 0,0,0, 0,0,0},
    // 2: thumb up
    {{0,0,0.1f, 0,0,0.9f, 0,0,0.9f, 0,0,0.9f, 0,0,0.9f}, -45,0,0, 0,0,0},
    // ... 17 more gesture definitions ...
};

bool readSimulated(SensorData& data) {
    // Add noise ±0.05 to hall sensors
    // Add noise ±2.0 degrees to euler angles
    // Apply Kalman filtering (validates signal processing pipeline)
    // Auto-cycle gesture every 300 frames (3 seconds @ 100Hz)
    return true;
}
```

**Data Values**:
- **Hall sensors**: 0.1 (uncurled) or 0.9 (curled) + ±0.05 noise
- **Euler angles**: 0° to ±45° + ±2.0° noise
- **Gyro**: 0 deg/s (static gestures)
- After 2-second calibration (FeatureNormalizer), values normalized to [0, 1]

### 4. DM40B Multimeter Wiring Debug Guide

**File Created**: `docs/HARDWARE_WIRING_DEBUG_GUIDE_DM40B.md`  
**Purpose**: Systematic hardware verification procedure before I2C debugging

**Key Sections**:
- Power supply verification (VCC/GND continuity)
- I2C pull-up resistor measurement (2kΩ expected)
- GPIO → PCA9548A SDA/SCL connectivity
- PCA9548A /RST pin connection (must be HIGH)
- BNO085 power, I2C bus (CH5), address pins (PS0/PS1)
- I2C idle state voltage measurement (GPIO8/9 should be 3.3V)
- Safety precautions (resistance measurements only with power OFF)

**Usage**: User can independently verify all physical connections before concluding hardware issue

---

## Code Architecture Reference

### Data Flow (Simulation Mode)

```
ESP32-S3 Firmware (Simulation Mode)
────────────────────────────────────

Task_SensorRead (Core 1, 100Hz):
    ├─ SensorManager.readAll()
    │  ├─ PCA9548A.begin() → FAIL → _simulation_mode = true
    │  └─ readSimulated(data)
    │     ├─ Select gesture from gestures[20] array
    │     ├─ Add Gaussian noise (±0.05 for hall, ±2.0° for euler)
    │     ├─ Apply Kalman filtering (KalmanFilter1D)
    │     └─ Auto-cycle gesture every 300 frames
    │
    ├─ FeatureNormalizer.updateStats() [first 200 frames]
    ├─ FeatureNormalizer.normalize() [after calibration]
    ├─ SlidingWindow.push(normalized_features)
    └─ Serial CSV output → Edge Impulse data forwarder

Task_Inference (Core 0, 30Hz):
    └─ Placeholder — will load TFLite Micro model in Phase 3

Task_Comms (Core 0):
    └─ Placeholder — will implement BLE/UDP in Phase 4
```

### CSV Output Format

```
timestamp_us, hall0x, hall0y, hall0z, hall1x, ..., hall4z, roll, pitch, yaw, gx, gy, gz

Example:
250701, 0.1234, 0.0567, 0.0891, ...  # After normalization
```

**Note**: Large numbers like 250701 are **timestamps in microseconds**, not sensor values. Normalized sensor values should be in [0, 1] range.

---

## Debugging Methodology

### Hardware Verification Workflow

**Phase 1: Power Supply** (ESP32-S3 OFF → ON)
- Measure 3.3V rail voltage (should be 3.25-3.35V)
- Verify PCA9548A VCC/GND connections (0Ω continuity)

**Phase 2: Pull-Up Resistors** (ESP32-S3 OFF)
- Measure GPIO8 → 3.3V resistance (~2kΩ)
- Measure GPIO9 → 3.3V resistance (~2kΩ)
- Verify no shorts to ground

**Phase 3: I2C Bus Connectivity** (ESP32-S3 OFF)
- GPIO8 → PCA9548A SDA (0Ω)
- GPIO9 → PCA9548A SCL (0Ω)
- No cross-short between SDA/SCL

**Phase 4: Idle State Verification** (ESP32-S3 ON)
- GPIO8 voltage = 3.3V (pull-up working)
- GPIO9 voltage = 3.3V (pull-up working)

**Phase 5: Device-Specific Checks**
- PCA9548A /RST pin → 3.3V (must be HIGH)
- BNO085 PS0/PS1 → GND (address 0x4A)

**Tools Required**:
- DM40B digital multimeter (or equivalent)
- Resistance mode: 200Ω and 20kΩ ranges
- DC voltage mode: 20V range

**Safety**: Always measure resistance with **power OFF** to avoid damaging multimeter

---

## Next Steps

### Immediate Actions (Hardware Track)

1. **Hardware Verification** (User action):
   - Follow `docs/HARDWARE_WIRING_DEBUG_GUIDE_DM40B.md`
   - Use DM40B multimeter to verify all physical connections
   - Check if PCA9548A and BNO085 modules show power LEDs
   - Measure GPIO8/9 idle voltage (should be 3.3V)

2. **If Wiring Verified → Re-test I2C**:
   - Power cycle ESP32-S3
   - Run minimal I2C scanner (addresses 0x70, 0x4A only)
   - If PCA9548A responds → test BNO085 on Channel 5
   - If still fails → try replacement modules or different I2C speed (100kHz instead of 400kHz)

3. **TMAG5273 Sensor Arrival** (Future):
   - Connect 5 sensors to Channels 0-4
   - Add 4.7kΩ pull-up resistors on each sub-channel
   - Verify all sensors respond at address 0x22
   - Switch from simulation mode to real sensor pipeline

### Immediate Actions (Software Track — Can Proceed Now)

1. **Edge Impulse Data Collection** (Simulation Mode):
   ```bash
   # Install Edge Impulse CLI
   npm install -g edge-impulse-cli
   
   # Start data forwarder with simulation firmware
   edge-impulse-data-forwarder --baud-rate 115200 --frequency 100
   
   # Firmware will auto-cycle through 20 gestures
   # Collect 3s × 20 gestures = 60 seconds minimum per gesture class
   # Recommended: 30-60 samples per gesture (900-1800 seconds total)
   ```

2. **Train 1D-CNN Model**:
   - Upload collected data to Edge Impulse project
   - Design impulse: 1D-CNN + Attention architecture
   - Train model (target: >90% accuracy on 20 classes)
   - Export Arduino library

3. **Integrate into Firmware**:
   - Copy Edge Impulse exported library to `glove_firmware/lib/Models/EI_Model/`
   - Modify `ModelRegistry` to load EI model
   - Implement `Task_Inference` to run model on SlidingWindow data
   - Test inference latency (target: <3ms per frame)

4. **Model Benchmark Comparison** (Phase 3.5):
   - Compare Edge Impulse 1D-CNN vs. custom TFLite Micro models
   - Evaluate accuracy, latency, memory usage
   - Select optimal model for deployment

### Long-Term Actions (Phases 4-7)

- **Phase 4**: BLE provisioning + WiFi UDP communication
- **Phase 5**: Python Relay + L2 ST-GCN inference + NLP + TTS
- **Phase 6**: Web frontend (React + R3F) / Unity Pro skeleton
- **Phase 7**: End-to-end integration testing

---

## Files Modified

| File | Status | Changes |
|------|--------|---------|
| `glove_firmware/platformio.ini` | Modified | Added `-DARDUINO_USB_CDC_ON_BOOT=1` |
| `glove_firmware/lib/Sensors/SensorManager.h` | Modified | Added simulation mode, 20 gesture signatures |
| `/etc/udev/rules.d/99-platformio-udev.rules` | Created | Permanent serial port access rules |
| `docs/HARDWARE_WIRING_DEBUG_GUIDE_DM40B.md` | Created | Multimeter wiring verification guide |

---

## Performance Targets

| Metric | Target | Current Status |
|--------|--------|----------------|
| Sensor sampling rate | 100Hz | ✓ Operational (simulation mode) |
| Calibration duration | 2s (200 frames) | ✓ Operational |
| Feature normalization | [0, 1] range | ✓ Operational |
| Gesture classes | 20 (initial) | ✓ Operational (simulation) |
| L1 inference latency | <3ms | Pending (Phase 3 implementation) |
| L1 accuracy | >90% Top-1 | Pending (after Edge Impulse training) |

---

## Known Issues

1. **I2C Devices Not Responding**:
   - PCA9548A (0x70) and BNO085 (0x4A) not detected
   - Requires hardware wiring verification with multimeter
   - Simulation mode allows development to proceed independently

2. **Pull-Up Resistor Value**:
   - Used 2kΩ instead of SOP-recommended 2kΩ
   - May cause slower rise time on I2C bus
   - Could affect communication reliability at 400kHz
   - Consider switching to 2kΩ if I2C issues persist

3. **Sub-Channel Pull-Ups**:
   - Not installed yet (TMAG5273 sensors not connected)
   - Will need 4.7kΩ pull-ups on each downstream channel
   - Wait until sensors arrive before adding

---

## Reference Documents

- **SOP Specification**: `docs/SOP_SPEC_PLAN_V3.md` (938 lines)
- **Claude Code Prompts**: `docs/CLAUDE_CODE_PROMPTS_V3.md` (28 prompts)
- **Hardware Assembly Guide**: `docs/HARDWARE_ASSEMBLY_GUIDE.md`
- **Wiring Debug Guide**: `docs/HARDWARE_WIRING_DEBUG_GUIDE_DM40B.md` (created this session)
- **Project Instructions**: `CLAUDE.md` (project root)
- **Progress Tracking**: `PROGRESS.md` (should be updated after this session)

---

## Session Summary

**What Changed**:
- Firmware now supports simulation mode for development without physical sensors
- Hardware debugging guide created for independent troubleshooting
- Serial port access fixed permanently with udev rules
- USB CDC initialization enabled for ESP32-S3 boot output

**What's Next**:
- Hardware verification with DM40B multimeter (user action)
- Edge Impulse data collection with simulation mode (can proceed immediately)
- Model training and integration (Phase 3 core work)

**Status**: Firmware operational in simulation mode, hardware debugging in progress, software development unblocked

---

**Generated**: 2026-05-20  
**Session Duration**: ~2 hours  
**Primary Outcome**: Simulation mode implementation + hardware debug guide  
**Blocking Issue**: I2C devices not responding (requires physical verification)  
**Parallel Work Enabled**: Edge Impulse data collection can proceed with synthetic data