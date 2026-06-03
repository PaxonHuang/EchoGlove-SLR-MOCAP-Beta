# EchoGlove V4 — Claude Code Prompts

> **Purpose:** Self-contained, actionable prompts for migrating EchoGlove from V3 (Hall+Magnet) to V4 (Flex+IMU6DOF).  
> **Usage:** Copy each prompt directly into Claude Code. Prompts are independent but should be executed in order.  
> **Prepend Prompt 00 to every subsequent prompt.**  

---

## Prompt 00: Project Context & Constraints

> **Copy this block and prepend to every prompt below.**

```
[ECHOGLOVE PROJECT CONTEXT]

You are working on EchoGlove — a smart data glove for Chinese Sign Language (CSL) recognition.

PROJECT STRUCTURE:
- glove_firmware/          — ESP32-S3 firmware (ESP-IDF 5.x, PlatformIO, FreeRTOS)
  - components/            — Reusable ESP-IDF components
    - ads1115/             — ADS1115 16-bit ADC driver (NEW in V4)
    - bno085/              — BNO085 IMU driver (modified for V4)
    - flex_manager/        — Flex sensor management (NEW in V4)
    - sensor_manager/      — Sensor coordination task (rewritten for V4)
    - kalman_filter/       — Kalman filter (modified for V4)
    - sliding_window/      — Sliding window buffer (modified for V4)
    - l1_inference/        — L1 edge model inference
    - protobuf_encoder/    — Protobuf encoding
    - udp_sender/          — UDP packet transmission
    - tmag5273/            — Hall effect sensor driver (DELETE in V4)
    - tca9548a/            — I2C MUX driver (DELETE in V4)
  - main/                  — Main application
  - proto/                 — Protobuf schema definitions
  - models/                — ML model files (TFLite INT8)
  - CMakeLists.txt
  - platformio.ini
- glove_relay/             — Python FastAPI relay server
  - glove_relay/
    - main.py              — FastAPI entry point
    - udp_receiver.py      — UDP packet receiver
    - protocol_parser.py   — V3/V4 protocol parser
    - st_gcn_mapper.py     — Sensor→skeleton mapper
    - l2_inference.py      — L2 ST-GCN model
    - nlp_corrector.py     — CSL NLP correction
    - tts_engine.py        — TTS translation
    - websocket_server.py  — WebSocket server
  - models/                — L2 model files
  - requirements.txt
  - Dockerfile
- glove_web/               — React + R3F frontend
  - src/
    - components/
      - HandSkeleton.jsx   — 3D hand rendering
      - SensorDisplay.jsx  — Real-time sensor values
    - pages/
      - LiveView.jsx       — Main live view
      - DatasetCollection.jsx — Dataset collection tool
    - hooks/
      - useWebSocket.js    — WebSocket connection hook
  - package.json
- glove_unity/             — Unity ms-MANO renderer
  - Assets/
    - Scripts/
      - ManoMapper.cs      — Sensor→MANO mapping
      - WebSocketClient.cs — WebSocket client
  - ProjectSettings/

HARDWARE (V4):
- ESP32-S3-WROOM-1 (dual-core 240MHz, 8MB PSRAM)
- BNO085 IMU @ I2C 0x4B (6DOF Game Rotation Vector mode)
- 2× ADS1115 @ I2C 0x48 and 0x49 (16-bit ADC)
- 5× Flex sensors with 47kΩ voltage dividers
- I2C bus: GPIO8 (SDA), GPIO9 (SCL), 400kHz

FEATURE VECTOR (V4):
- 11 dimensions per frame: [5 flex%, 3 euler°, 3 gyro°/s]
- 100Hz sampling rate
- 30-frame sliding window → 330-dim L1 input

CONSTRAINTS:
- L1 model: <50K params, INT8, <10ms inference on ESP32-S3
- All I2C access must be mutex-protected (shared bus)
- FreeRTOS tasks pinned to appropriate cores
- PlatformIO build system with ESP-IDF 5.x
- Protobuf over UDP (firmware→relay), WebSocket JSON (relay→frontend)
```

---

## Prompt 01: ADS1115 Driver

```
[TASK] Create an ESP-IDF driver for the ADS1115 16-bit I2C ADC.

[CONTEXT FROM PROMPT 00]
(Prepend Prompt 00 here)

[INPUT]
- I2C port: I2C_NUM_0
- Two instances: ADDR1 = 0x48 (ADDR pin → GND), ADDR2 = 0x49 (ADDR pin → VCC)
- GPIO8 = SDA, GPIO9 = SCL, 400kHz bus speed

[OUTPUT]
Create the following files in glove_firmware/components/ads1115/:

1. `ads1115.h` — Header with:
   - Register definitions (0x00 Conversion, 0x01 Config, 0x02 Lo_thresh, 0x03 Hi_thresh)
   - Config register bit fields (OS, MUX, PGA, MODE, DR, COMP_QUE, etc.)
   - PGA enum: ±6.144V, ±4.096V, ±2.048V, ±1.024V, ±0.512V, ±0.256V
   - Data rate enum: 8, 16, 32, 64, 128, 250, 475, 860 SPS
   - MUX enum for single-ended channels AIN0-AIN3
   - Opaque handle type `ads1115_handle_t`
   - Function prototypes (see API below)

2. `ads1115.c` — Implementation with:
   - `ads1115_init(i2c_port_t port, uint8_t addr, ads1115_handle_t *handle)` — verify device present (read config register)
   - `ads1115_read_channel(handle, channel, &raw_value)` — single-shot or continuous mode
   - `ads1115_read_all_channels(handle, raw[4])` — read all 4 channels sequentially
   - `ads1115_set_pga(handle, pga)` — set programmable gain
   - `ads1115_set_data_rate(handle, rate)` — set sampling rate
   - `ads1115_start_continuous(handle)` — enter continuous mode
   - `ads1115_stop_continuous(handle)` — enter single-shot mode
   - Mutex protection for all I2C operations (xSemaphoreCreateMutex, take/give around i2c calls)
   - Error handling: timeout (100ms), NACK recovery, bus reset

3. `CMakeLists.txt` — ESP-IDF component build file

4. `Kconfig` — Menuconfig options for I2C port, addresses, default PGA, default data rate

[REQUIREMENTS]
- Use ESP-IDF i2c_master or legacy i2c_driver_install API
- Thread-safe: all public functions must acquire mutex before I2C access
- Default PGA: ±4.096V (best for flex sensor 0.5–3.0V range)
- Default data rate: 860 SPS
- Support both single-shot and continuous conversion modes
- Config register write must set COMP_QUE to disabled (bits [1:0] = 11)
- Conversion register is 16-bit signed big-endian

[TEST]
- Write a test in `glove_firmware/components/ads1115/test/test_ads1115.c`
- Test: init succeeds with valid address
- Test: read_channel returns values in expected range for known voltage
- Test: read_all_channels returns 4 values
- Test: mutex prevents concurrent access (simulate two tasks)
- Test: init fails gracefully with invalid address (NACK)
```

---

## Prompt 02: BNO085 6DOF Mode

```
[TASK] Modify the BNO085 driver to use 6DOF Game Rotation Vector mode (no magnetometer).

[CONTEXT FROM PROMPT 00]
(Prepend Prompt 00 here)

[INPUT]
- Existing file: glove_firmware/components/bno085/bno085.c and bno085.h
- Current mode: SH2_ROTATION_VECTOR (9DOF with magnetometer)
- I2C address: 0x4B
- Interrupt pin: GPIO11

[OUTPUT]
Modify the following files:

1. `bno085.h` — Add/update:
   - `BNO085_MODE_GAME_ROTATION_VECTOR` enum value
   - `bno085_get_euler(handle, &roll, &pitch, &yaw)` — quaternion to euler conversion
   - `bno085_get_gyro(handle, &gx, &gy, &gz)` — angular velocity
   - `bno085_get_all_6dof(handle, &euler[3], &gyro[3])` — combined read

2. `bno085.c` — Modify:
   - Change initialization to enable SH2_GAME_ROTATION_VECTOR (report ID 0x05)
   - Disable SH2_MAGNETIC_FIELD_CALIBRATED (report ID 0x02)
   - Disable SH2_ROTATION_VECTOR (report ID 0x01) — use Game RV instead
   - Add quaternion → euler conversion function:
     ```c
     void quaternion_to_euler(float w, float x, float y, float z, 
                              float *roll, float *pitch, float *yaw) {
         // Roll (x-axis rotation)
         float sinr_cosp = 2.0f * (w * x + y * z);
         float cosr_cosp = 1.0f - 2.0f * (x * x + y * y);
         *roll = atan2f(sinr_cosp, cosr_cosp);
         
         // Pitch (y-axis rotation)
         float sinp = 2.0f * (w * y - z * x);
         if (fabsf(sinp) >= 1.0f)
             *pitch = copysignf(M_PI / 2.0f, sinp);
         else
             *pitch = asinf(sinp);
         
         // Yaw (z-axis rotation)
         float siny_cosp = 2.0f * (w * z + x * y);
         float cosy_cosp = 1.0f - 2.0f * (y * y + z * z);
         *yaw = atan2f(siny_cosp, cosy_cosp);
         
         // Convert to degrees
         *roll *= 180.0f / M_PI;
         *pitch *= 180.0f / M_PI;
         *yaw *= 180.0f / M_PI;
     }
     ```
   - Keep interrupt-driven data ready detection (GPIO11)
   - Add calibration status check (Game RV uses accel+gyro cal only)

3. Update `bno085_init()` to accept mode parameter or default to Game RV

[REQUIREMENTS]
- Game Rotation Vector report interval: 10ms (100Hz) to match sampling rate
- Gyroscope report: enable at 100Hz for angular velocity
- Magnetometer: completely disabled (no reports, no calibration)
- Quaternion normalization check: warn if |q| deviates from 1.0 by >0.05
- Thread-safe: mutex for I2C access (shared with ADS1115)
- Keep existing interrupt-driven architecture

[TEST]
- Test: init in Game RV mode succeeds
- Test: quaternion values are valid (|q| ≈ 1.0)
- Test: euler conversion produces correct values for known quaternions
- Test: gyro values are non-zero when device is rotating
- Test: magnetometer reports are NOT generated (verify no report ID 0x02)
```

---

## Prompt 03: Flex Sensor Module

```
[TASK] Create a flex sensor management module with calibration, EMA filtering, and NVS persistence.

[CONTEXT FROM PROMPT 00]
(Prepend Prompt 00 here)

[INPUT]
- 5 flex sensors read via 2× ADS1115
  - ADS1115 #1 (0x48): CH0=Thumb, CH1=Index, CH2=Middle, CH3=Ring
  - ADS1115 #2 (0x49): CH0=Pinky
- Voltage divider: 3.3V → [47kΩ] → ADC input → [Flex] → GND
- ADC range: ~9000 (unbent) to ~19000 (fully bent) at 16-bit, PGA ±4.096V
- Calibration button: GPIO13 (active low)

[OUTPUT]
Create the following files in glove_firmware/components/flex_manager/:

1. `flex_manager.h` — Header with:
   - `FLEX_COUNT` = 5
   - `flex_config_t` struct: ads1115 handles, EMA alpha, calibration thresholds
   - `flex_calibration_t` struct: min_adc[FLEX_COUNT], max_adc[FLEX_COUNT], calibrated flag
   - Function prototypes (see API below)

2. `flex_manager.c` — Implementation with:
   - `flex_manager_init(config)` — initialize, load calibration from NVS if available
   - `flex_manager_calibrate_start()` — reset min/max tracking
   - `flex_manager_calibrate_sample()` — read current ADC, update min/max
   - `flex_manager_calibrate_end()` — save calibration to NVS (namespace: "flex_cal")
   - `flex_manager_read(percentages[5])` — read ADC, apply calibration, apply EMA filter, return 0-100%
   - `flex_manager_load_calibration()` — load from NVS
   - `flex_manager_save_calibration()` — save to NVS
   - `flex_manager_is_calibrated()` — check if valid calibration exists
   - `flex_manager_get_raw(raw[5])` — return raw ADC values (for debugging)
   
   EMA filter implementation:
   ```c
   // Exponential Moving Average: filtered = alpha * new + (1-alpha) * filtered
   static float ema_filter(float new_value, float filtered, float alpha) {
       return alpha * new_value + (1.0f - alpha) * filtered;
   }
   ```
   
   Percentage normalization:
   ```c
   static float adc_to_percent(int16_t adc, int16_t min_adc, int16_t max_adc) {
       if (max_adc <= min_adc) return 0.0f;
       float pct = (float)(adc - min_adc) / (float)(max_adc - min_adc) * 100.0f;
       return fminf(100.0f, fmaxf(0.0f, pct));  // Clamp to [0, 100]
   }
   ```

3. `CMakeLists.txt` — Component build file, depends on ads1115 and nvs_flash

[REQUIREMENTS]
- NVS namespace: "flex_cal"
- NVS keys: "min0"..."min4", "max0"..."max4", "cal_flag"
- EMA alpha: 0.15 (configurable via menuconfig)
- Default calibration values (if no NVS data): min=8000, max=20000
- Fault detection: if ADC value < 1000 or > 60000, flag sensor fault
- Thread-safe: mutex for calibration state
- Calibration button (GPIO13): press to start 30-second calibration sequence
  - During calibration: sample at 50Hz, track min/max
  - After 30s: save to NVS, signal completion via LED

[TEST]
- Test: init loads default calibration when NVS is empty
- Test: calibrate_sample correctly updates min/max
- Test: calibrate_end persists to NVS
- Test: read returns 0% for min ADC, 100% for max ADC
- Test: EMA filter smooths noisy input (verify output is less noisy)
- Test: fault detection flags out-of-range values
- Test: load/save calibration round-trip through NVS
```

---

## Prompt 04: Remove V3 Hall Stack

```
[TASK] Remove all Hall-effect sensor and I2C MUX code from the EchoGlove V3 firmware.

[CONTEXT FROM PROMPT 00]
(Prepend Prompt 00 here)

[INPUT]
- Files to delete:
  - glove_firmware/components/tmag5273/ (entire directory)
    - tmag5273.c
    - tmag5273.h
    - CMakeLists.txt
  - glove_firmware/components/tca9548a/ (entire directory)
    - tca9548a.c
    - tca9548a.h
    - CMakeLists.txt

[OUTPUT]
1. Delete the above files and directories.

2. Search the entire glove_firmware/ directory for references to:
   - `tmag5273` — remove #include, function calls, variable declarations
   - `tca9548a` — remove #include, function calls, variable declarations
   - `hall_value` or `hall_sensor` — remove or replace with flex equivalents
   - `mux_channel` or `mux_select` — remove MUX channel selection code
   - `N52` or `magnet` — remove any magnet-related comments or config

3. Update `glove_firmware/main/CMakeLists.txt` to remove tmag5273 and tca9548a from PRIV_REQUIRES

4. Update `glove_firmware/main/main.c` or `app_main()`:
   - Remove Hall sensor initialization code
   - Remove TCA9548A MUX initialization code
   - Remove any Hall-related task creation
   - Keep BNO085 initialization (will be modified in Prompt 02)

5. Update `glove_firmware/platformio.ini`:
   - Remove any lib_deps related to tmag5273 or tca9548a

6. Create a migration log file: `glove_firmware/MIGRATION_V4_LOG.md` listing all deleted files and removed references

[REQUIREMENTS]
- Verify no remaining references to deleted code (grep for tmag5273, tca9548a)
- Do NOT delete BNO085 code (it's being modified, not removed)
- Do NOT modify the Kalman filter yet (dimensions change in a separate prompt)
- Preserve all code comments explaining what was removed and why
- Run `pio build` after changes to verify compilation succeeds (ignoring missing sensor_manager references that will be updated later)

[TEST]
- Verify: `grep -r "tmag5273" glove_firmware/` returns no results
- Verify: `grep -r "tca9548a" glove_firmware/` returns no results
- Verify: `pio build` does not fail on tmag5273/tca9548a references
- Verify: MIGRATION_V4_LOG.md lists all changes
```

---

## Prompt 05: Protobuf V4 Schema & Code Generation

```
[TASK] Update the Protobuf schema to V4 and regenerate code.

[CONTEXT FROM PROMPT 00]
(Prepend Prompt 00 here)

[INPUT]
- Existing schema: glove_firmware/proto/sensor_frame.proto (V3)
- V3 fields: hall_values[5], imu_quaternion[4], imu_accel[3], imu_gyro[3]

[OUTPUT]
1. Update `glove_firmware/proto/sensor_frame.proto`:
   ```protobuf
   syntax = "proto3";
   package echoglove;

   message SensorFrame {
       uint32 timestamp_ms = 1;
       uint32 version = 2;              // 4 for V4
       
       // Flex sensor values (0-100%)
       repeated float flex_values = 3;  // [thumb, index, middle, ring, pinky]
       
       // IMU data (6DOF)
       repeated float imu_euler = 4;    // [roll, pitch, yaw] degrees
       repeated float imu_gyro = 5;     // [gx, gy, gz] °/s
       
       // L1 result
       uint32 l1_class_id = 6;
       float l1_confidence = 7;
       
       // System
       uint32 battery_mv = 8;
       uint32 wifi_rssi = 9;
       uint32 uptime_s = 10;
   }
   ```

2. Generate C code using nanopb:
   ```
   protoc --nanopb_out=glove_firmware/generated/ sensor_frame.proto
   ```
   
   Or if using nanopb generator:
   ```
   python nanopb/generator/nanopb_generator.py sensor_frame.proto
   ```

3. Update `glove_firmware/components/protobuf_encoder/protobuf_encoder.c`:
   - Change encoding to fill `flex_values` instead of `hall_values`
   - Change encoding to fill `imu_euler` (3 values) instead of `imu_quaternion` (4 values)
   - Remove `imu_accel` field encoding
   - Keep `imu_gyro` encoding (3 values)
   - Set `version = 4`

4. Update `glove_relay/glove_relay/sensor_frame_pb2.py`:
   - Regenerate Python protobuf code:
     ```
     protoc --python_out=glove_relay/glove_relay/ sensor_frame.proto
     ```

5. Update `glove_relay/glove_relay/protocol_parser.py`:
   - Add V4 parsing: extract `flex_values[5]`, `imu_euler[3]`, `imu_gyro[3]`
   - Keep V3 parsing for backward compatibility
   - Auto-detect version from `version` field

[REQUIREMENTS]
- Maintain backward compatibility with V3 protocol
- Version field must be the first field after timestamp
- Use float (not double) for sensor values to minimize packet size
- Target UDP packet size: <128 bytes (fits in single MTU)
- nanopb options file should set max_size for repeated fields

[TEST]
- Test: protobuf compilation succeeds for both C and Python
- Test: encode V4 frame → decode → values match (round-trip)
- Test: decode V3 frame still works (backward compatibility)
- Test: version field correctly distinguishes V3 from V4
- Test: UDP packet size < 128 bytes
```

---

## Prompt 06: Core1 Sampling Task Rewrite

```
[TASK] Rewrite the Core1 sensor sampling task for V4 architecture (100Hz, ADS1115+BNO085).

[CONTEXT FROM PROMPT 00]
(Prepend Prompt 00 here)

[INPUT]
- ADS1115 #1 (0x48): Flex sensors thumb, index, middle, ring
- ADS1115 #2 (0x49): Flex sensor pinky
- BNO085 (0x4B): 6DOF Game Rotation Vector + Gyro
- Target: 100Hz (10ms period), <8ms execution time per frame
- I2C bus: shared, mutex-protected

[OUTPUT]
Rewrite `glove_firmware/components/sensor_manager/sensor_manager.c` and `.h`:

1. `sensor_manager.h`:
   ```c
   #define SENSOR_DIMS 11  // 5 flex + 3 euler + 3 gyro
   
   typedef struct {
       float flex[5];      // 0-100%
       float euler[3];     // roll, pitch, yaw (degrees)
       float gyro[3];      // gx, gy, gz (°/s)
   } sensor_frame_t;
   
   esp_err_t sensor_manager_init(void);
   esp_err_t sensor_manager_read_frame(sensor_frame_t *frame);
   ```

2. `sensor_manager.c`:
   ```c
   void sensor_manager_task(void *pvParameters) {
       TickType_t xLastWakeTime = xTaskGetTickCount();
       sensor_frame_t frame;
       
       while (1) {
           int64_t start = esp_timer_get_time();
           
           // 1. Read flex sensors via ADS1115 (~2ms for both instances)
           float flex_values[5];
           flex_manager_read(flex_values);
           
           // 2. Read BNO085 quaternion + gyro (~1ms)
           float quat[4], gyro[3];
           bno085_get_quaternion(imu_handle, quat);
           bno085_get_gyro(imu_handle, &gyro[0], &gyro[1], &gyro[2]);
           
           // 3. Convert quaternion → euler (~0.01ms)
           float euler[3];
           quaternion_to_euler(quat[0], quat[1], quat[2], quat[3],
                              &euler[0], &euler[1], &euler[2]);
           
           // 4. Assemble frame
           memcpy(frame.flex, flex_values, sizeof(float) * 5);
           memcpy(frame.euler, euler, sizeof(float) * 3);
           memcpy(frame.gyro, gyro, sizeof(float) * 3);
           
           // 5. Push to Kalman filter queue
           xQueueSend(kalman_queue, &frame, 0);
           
           // 6. Timing check
           int64_t elapsed_us = esp_timer_get_time() - start;
           if (elapsed_us > 8000) {
               ESP_LOGW(TAG, "Frame overrun: %lld us", elapsed_us);
           }
           
           // 7. Wait for next 10ms tick
           vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(10));
       }
   }
   ```

3. Pin task to Core 1:
   ```c
   xTaskCreatePinnedToCore(
       sensor_manager_task,
       "sensor_mgr",
       4096,           // stack size
       NULL,
       5,              // priority (high)
       NULL,
       1               // Core 1
   );
   ```

[REQUIREMENTS]
- 100Hz sampling rate with <2% jitter
- <8ms execution time per frame (80% of 10ms budget)
- All I2C operations mutex-protected
- Error handling: if any sensor read fails, use last valid value + log warning
- Flex sensor reads should happen before BNO085 (ADS1115 is slower, start first)
- Push assembled frame to FreeRTOS queue for Kalman filter consumer
- Log frame rate every 10 seconds for monitoring

[TEST]
- Test: task creates and runs at 100Hz (verify with esp_timer)
- Test: frame contains valid data in all 11 dimensions
- Test: execution time < 8ms (measure with esp_timer_get_time)
- Test: sensor read failure does not crash task (inject I2C error)
- Test: queue receives frames at expected rate
- Test: frame overrun warning appears when simulated delay exceeds 8ms
```

---

## Prompt 07: Python Relay Parser Update

```
[TASK] Update the Python relay server to support V3/V4 auto-detection and V4 parsing.

[CONTEXT FROM PROMPT 00]
(Prepend Prompt 00 here)

[INPUT]
- Existing file: glove_relay/glove_relay/protocol_parser.py
- V3 format: hall_values[5], imu_quaternion[4], imu_accel[3], imu_gyro[3]
- V4 format: flex_values[5], imu_euler[3], imu_gyro[3], version=4

[OUTPUT]
1. Update `glove_relay/glove_relay/protocol_parser.py`:
   ```python
   from dataclasses import dataclass
   from typing import Optional
   import struct
   
   @dataclass
   class SensorData:
       version: int
       timestamp_ms: int
       # Finger data (unified field — flex% for V4, Hall mT for V3)
       finger_values: list[float]  # 5 values
       # IMU data (unified — euler for V4, quaternion+accel for V3)
       euler: list[float]          # 3 values [roll, pitch, yaw]
       gyro: list[float]           # 3 values [gx, gy, gz]
       # L1 result
       l1_class_id: int
       l1_confidence: float
       # System
       battery_mv: int
       wifi_rssi: int
       uptime_s: int
       # Legacy V3 fields (None for V4)
       quaternion: Optional[list[float]] = None  # V3 only
       accel: Optional[list[float]] = None       # V3 only
   
   class ProtocolParser:
       def __init__(self):
           self.v3_count = 0
           self.v4_count = 0
       
       def parse(self, data: bytes) -> SensorData:
           # Try protobuf decode first
           try:
               frame = SensorFrame()
               frame.ParseFromString(data)
               
               if frame.version == 4:
                   return self._parse_v4(frame)
               elif frame.version == 3:
                   return self._parse_v3(frame)
               else:
                   # Heuristic: check field presence
                   if len(frame.flex_values) > 0:
                       return self._parse_v4(frame)
                   else:
                       return self._parse_v3(frame)
           except Exception as e:
               raise ValueError(f"Failed to parse packet: {e}")
       
       def _parse_v4(self, frame) -> SensorData:
           self.v4_count += 1
           return SensorData(
               version=4,
               timestamp_ms=frame.timestamp_ms,
               finger_values=list(frame.flex_values),
               euler=list(frame.imu_euler),
               gyro=list(frame.imu_gyro),
               l1_class_id=frame.l1_class_id,
               l1_confidence=frame.l1_confidence,
               battery_mv=frame.battery_mv,
               wifi_rssi=frame.wifi_rssi,
               uptime_s=frame.uptime_s,
           )
       
       def _parse_v3(self, frame) -> SensorData:
           self.v3_count += 1
           # Convert V3 quaternion to euler for unified downstream
           quat = list(frame.imu_quaternion)
           euler = quaternion_to_euler(quat[0], quat[1], quat[2], quat[3])
           return SensorData(
               version=3,
               timestamp_ms=frame.timestamp_ms,
               finger_values=list(frame.hall_values),
               euler=euler,
               gyro=list(frame.imu_gyro),
               l1_class_id=frame.l1_class_id,
               l1_confidence=frame.l1_confidence,
               battery_mv=frame.battery_mv,
               wifi_rssi=frame.wifi_rssi,
               uptime_s=frame.uptime_s,
               quaternion=quat,
               accel=list(frame.imu_accel),
           )
   ```

2. Update `glove_relay/glove_relay/st_gcn_mapper.py`:
   - Change input field from `hall_values` to `finger_values`
   - Flex% (0-100) needs different scaling than Hall mT for skeleton mapping
   - Update normalization constants

3. Add logging: log which protocol version is being used (V3 vs V4) every 100 packets

[REQUIREMENTS]
- Backward compatible with V3
- Auto-detect V3/V4 based on version field
- Unified `SensorData` output for downstream (L2, NLP, TTS)
- V3→V4 conversion: quaternion→euler conversion for V3 packets
- Error handling: graceful degradation for malformed packets
- Unit tests for both V3 and V4 parsing

[TEST]
- Test: parse valid V4 packet → correct SensorData
- Test: parse valid V3 packet → correct SensorData with euler conversion
- Test: auto-detect correctly identifies V3 vs V4
- Test: malformed packet raises ValueError
- Test: V3→V4 euler conversion matches expected values
```

---

## Prompt 08: Web Frontend R3F Skeleton Update

```
[TASK] Update the React Three Fiber hand skeleton to use flex percentage values for bone angles.

[CONTEXT FROM PROMPT 00]
(Prepend Prompt 00 here)

[INPUT]
- Existing file: glove_web/src/components/HandSkeleton.jsx
- V3 input: hall_values[5] (magnetic field strength)
- V4 input: flex_percentages[5] (0-100%)
- WebSocket message format: { finger_values: [5], euler: [3], gyro: [3] }

[OUTPUT]
1. Update `glove_web/src/components/HandSkeleton.jsx`:

   ```jsx
   import React, { useRef, useMemo } from 'react';
   import { useFrame } from '@react-three/fiber';
   import * as THREE from 'three';
   
   // Flex percentage to bone angle mapping
   function flexToAngle(flexPercent, fingerIndex) {
       // MCP joint: 0% → 0°, 100% → 90°
       const mcpAngle = (flexPercent / 100) * (Math.PI / 2);
       
       // Finger-specific kinematic ratios
       const ratios = {
           0: { mcp: 1.0, pip: 0.8, dip: 0.6 },  // Thumb
           1: { mcp: 1.0, pip: 0.8, dip: 0.6 },  // Index
           2: { mcp: 1.0, pip: 0.8, dip: 0.6 },  // Middle
           3: { mcp: 1.0, pip: 0.8, dip: 0.6 },  // Ring
           4: { mcp: 1.0, pip: 0.8, dip: 0.6 },  // Pinky
       };
       
       const r = ratios[fingerIndex] || ratios[1];
       return {
           mcp: mcpAngle * r.mcp,
           pip: mcpAngle * r.pip,
           dip: mcpAngle * r.dip,
       };
   }
   
   // Thumb has different kinematics (CMC joint)
   function thumbToAngles(flexPercent) {
       const base = (flexPercent / 100) * (Math.PI / 2);
       return {
           cmc: base * 0.5,   // Carpometacarpal
           mcp: base * 0.8,   // Metacarpophalangeal
           ip: base * 0.7,    // Interphalangeal
       };
   }
   
   function HandSkeleton({ sensorData }) {
       const groupRef = useRef();
       
       // Apply wrist rotation from IMU euler
       useFrame(() => {
           if (!groupRef.current || !sensorData?.euler) return;
           const [roll, pitch, yaw] = sensorData.euler;
           groupRef.current.rotation.set(
               THREE.MathUtils.degToRad(pitch),
               THREE.MathUtils.degToRad(yaw),
               THREE.MathUtils.degToRad(roll)
           );
       });
       
       const fingerAngles = useMemo(() => {
           if (!sensorData?.finger_values) return null;
           return sensorData.finger_values.map((flex, i) => {
               if (i === 0) return thumbToAngles(flex);
               return flexToAngle(flex, i);
           });
       }, [sensorData?.finger_values]);
       
       return (
           <group ref={groupRef}>
               {/* Wrist */}
               <mesh position={[0, 0, 0]}>
                   <sphereGeometry args={[0.5, 16, 16]} />
                   <meshStandardMaterial color="#666" />
               </mesh>
               
               {/* Fingers */}
               {fingerAngles?.map((angles, i) => (
                   <Finger 
                       key={i} 
                       index={i} 
                       angles={angles}
                       position={[fingerPositions[i]]} 
                   />
               ))}
           </group>
       );
   }
   ```

2. Update `glove_web/src/hooks/useWebSocket.js`:
   - Parse incoming WebSocket messages
   - Extract `finger_values`, `euler`, `gyro`
   - Pass to HandSkeleton component via state

3. Update `glove_web/src/components/SensorDisplay.jsx`:
   - Display flex percentages (0-100%) instead of Hall values
   - Show euler angles (roll, pitch, yaw)
   - Real-time bar chart for flex values

[REQUIREMENTS]
- Smooth animation: interpolate between frames (lerp, 60fps)
- Flex 0% = fully extended finger, 100% = fully curled fist
- Wrist rotation from IMU euler (pitch/yaw/roll)
- Responsive layout for mobile and desktop
- Color coding: green (good data), yellow (stale), red (fault)

[TEST]
- Test: flex 0% renders fully extended finger
- Test: flex 100% renders fully curled finger
- Test: IMU euler rotates wrist correctly
- Test: smooth animation between frames
- Test: WebSocket connection handles reconnect
```

---

## Prompt 09: Dataset Collection Web Tool

```
[TASK] Create a web-based dataset collection tool for EchoGlove V4.

[CONTEXT FROM PROMPT 00]
(Prepend Prompt 00 here)

[INPUT]
- Sensor data via WebSocket: { finger_values[5], euler[3], gyro[3] }
- Target: 200 CSL gesture classes
- 50 repetitions per gesture per user

[OUTPUT]
Create `glove_web/src/pages/DatasetCollection.jsx`:

1. **Gesture Display:**
   - Show target gesture name + reference image/video
   - Countdown timer before recording starts
   - Recording indicator (red dot)

2. **Data Recording:**
   - Record sensor stream at 100Hz during gesture
   - Store as CSV: timestamp, flex0-4, euler0-2, gyro0-2, gesture_label, user_id
   - Minimum gesture duration: 0.5 seconds
   - Maximum gesture duration: 5 seconds
   - Auto-stop if hand returns to neutral pose

3. **Quality Validation:**
   - Check gesture duration (0.5–5s range)
   - Check flex range (at least one finger >30% flex)
   - Check for sensor dropouts (no zeros in stream)
   - Visual feedback: ✓ valid, ✗ invalid (re-record)

4. **Data Export:**
   - Export as CSV (one file per gesture)
   - Export as TFRecord (batch export all)
   - Include metadata: user_id, session_id, timestamp, gesture_label

5. **UI Layout:**
   ```
   ┌─────────────────────────────────────────────┐
   │  Dataset Collection Tool          [User: __] │
   ├──────────────┬──────────────────────────────┤
   │  Gesture     │                              │
   │  List        │     3D Hand Preview          │
   │              │     (real-time)              │
   │  □ 你好      │                              │
   │  □ 谢谢      │                              │
   │  □ 对不起    │                              │
   │  ...         │                              │
   │              │     ┌──────────────┐         │
   │  Progress:   │     │  Reference   │         │
   │  15/50 ✓     │     │  Image/Video │         │
   │              │     └──────────────┘         │
   ├──────────────┴──────────────────────────────┤
   │  [Start Recording] [Export CSV] [Export TF]  │
   │  Status: Ready | Recording: 0.0s | Valid: ✓  │
   └─────────────────────────────────────────────┘
   ```

6. **Backend API** (add to glove_relay):
   ```python
   # POST /api/dataset/sample
   # Body: { gesture_label, user_id, session_id, data: [[timestamp, flex0-4, euler0-2, gyro0-2], ...] }
   # Response: { sample_id, quality_score, valid: bool }
   
   # GET /api/dataset/export?format=csv&gesture=all&user=all
   # Response: ZIP file with CSV files
   
   # GET /api/dataset/stats
   # Response: { total_samples, per_gesture_counts, per_user_counts }
   ```

[REQUIREMENTS]
- Support 200 gesture labels (configurable)
- Real-time hand preview during recording
- Auto-detect gesture start/end (threshold-based)
- Store raw 100Hz data (no downsampling)
- Metadata: user_id, session_id, timestamp, hand_size_estimate
- Export formats: CSV, TFRecord, NumPy .npz
- Progress tracking: samples collected per gesture

[TEST]
- Test: recording starts and stops correctly
- Test: CSV export contains correct columns and data
- Test: quality validation rejects too-short gestures
- Test: quality validation accepts valid gestures
- Test: gesture list loads from config file
```

---

## Prompt 10: L1 Model Architecture Update

```
[TASK] Update the L1 edge inference model for V4's 330-dim input (30×11).

[CONTEXT FROM PROMPT 00]
(Prepend Prompt 00 here)

[INPUT]
- V3 model: 1D-CNN+Attention, 34K params, 450-dim input (30×15)
- V4 model: 1D-CNN+Attention, ~28K params, 330-dim input (30×11)
- 11 channels: [5 flex%, 3 euler°, 3 gyro°/s]
- Target: <50K params, INT8 quantized, <10ms on ESP32-S3

[OUTPUT]
1. Create `glove_firmware/models/l1_model_v4.py` (PyTorch training script):

   ```python
   import torch
   import torch.nn as nn
   
   class EchoGloveL1V4(nn.Module):
       def __init__(self, num_classes=200, window_size=30, num_channels=11):
           super().__init__()
           
           self.conv1 = nn.Conv1d(num_channels, 32, kernel_size=3, padding=1)
           self.bn1 = nn.BatchNorm1d(32)
           self.conv2 = nn.Conv1d(32, 64, kernel_size=3, padding=1)
           self.bn2 = nn.BatchNorm1d(64)
           self.conv3 = nn.Conv1d(64, 64, kernel_size=3, padding=1)
           self.bn3 = nn.BatchNorm1d(64)
           
           self.attention = nn.MultiheadAttention(
               embed_dim=64, num_heads=4, batch_first=True
           )
           self.attn_norm = nn.LayerNorm(64)
           
           self.pool = nn.AdaptiveAvgPool1d(1)
           
           self.fc1 = nn.Linear(64, 128)
           self.dropout1 = nn.Dropout(0.3)
           self.fc2 = nn.Linear(128, 64)
           self.dropout2 = nn.Dropout(0.2)
           self.fc3 = nn.Linear(64, num_classes)
           
           self.relu = nn.ReLU()
           
       def forward(self, x):
           # x: (batch, 30, 11) → transpose to (batch, 11, 30)
           x = x.transpose(1, 2)
           
           # Conv1D blocks
           x = self.relu(self.bn1(self.conv1(x)))
           x = self.relu(self.bn2(self.conv2(x)))
           x = self.relu(self.bn3(self.conv3(x)))
           
           # Attention (transpose for attention: batch, seq, features)
           x = x.transpose(1, 2)  # (batch, 30, 64)
           attn_out, _ = self.attention(x, x, x)
           x = self.attn_norm(x + attn_out)  # Residual
           
           # Pool and classify
           x = x.transpose(1, 2)  # (batch, 64, 30)
           x = self.pool(x).squeeze(-1)  # (batch, 64)
           
           x = self.relu(self.fc1(x))
           x = self.dropout1(x)
           x = self.relu(self.fc2(x))
           x = self.dropout2(x)
           x = self.fc3(x)
           
           return x
   
   # Count parameters
   model = EchoGloveL1V4()
   total_params = sum(p.numel() for p in model.parameters())
   print(f"Total parameters: {total_params:,}")
   # Expected: ~28,000
   ```

2. Create `glove_firmware/models/train_v4.py` (training script):
   - Load dataset (CSV or TFRecord from collection tool)
   - Data augmentation (time warp, noise, rotation)
   - Train/val split (80/20 stratified)
   - Adam optimizer, lr=1e-3, ReduceLROnPlateau
   - CrossEntropyLoss
   - 100 epochs, early stopping patience=15
   - Save best model checkpoint

3. Create `glove_firmware/models/export_v4.py` (INT8 quantization):
   ```python
   # PyTorch → ONNX → TFLite INT8
   # 1. Export to ONNX
   # 2. Convert ONNX to TFLite
   # 3. Post-training quantization with representative dataset
   # 4. Save as l1_model_v4_int8.tflite
   ```

4. Create `glove_firmware/models/benchmark_v4.py`:
   - Measure inference time on target hardware
   - Compare float32 vs INT8 accuracy
   - Generate confusion matrix

[REQUIREMENTS]
- Input: (batch, 30, 11) float32 tensor
- Output: (batch, num_classes) logits
- Parameters: <50K (target ~28K)
- INT8 quantization: <1% accuracy loss
- Inference time: <10ms on ESP32-S3 (measure with benchmark)
- Support variable num_classes (configurable)
- Data augmentation: time warping (±20%), noise (σ=0.01), rotation (±5°)

[TEST]
- Test: model forward pass succeeds with correct input/output shapes
- Test: parameter count < 50K
- Test: INT8 quantized model produces same top-1 predictions as float32 (within 1%)
- Test: training loop runs without errors
- Test: export produces valid TFLite file
```

---

## Prompt 11: ST-GCN L2 Model Update

```
[TASK] Update the ST-GCN L2 model for V4's flex sensor input.

[CONTEXT FROM PROMPT 00]
(Prepend Prompt 00 here)

[INPUT]
- V3: ST-GCN with 21-node hand skeleton, 280K params
- V4: Same skeleton, different input mapping (flex% instead of Hall mT)
- Skeleton: 21 nodes (wrist + 4 joints × 5 fingers)

[OUTPUT]
1. Update `glove_relay/glove_relay/st_gcn_mapper.py`:

   ```python
   # Hand skeleton graph (21 nodes)
   # Node 0: Wrist
   # Nodes 1-4: Thumb (CMC, MCP, IP, tip)
   # Nodes 5-8: Index (MCP, PIP, DIP, tip)
   # Nodes 9-12: Middle (MCP, PIP, DIP, tip)
   # Nodes 13-16: Ring (MCP, PIP, DIP, tip)
   # Nodes 17-20: Pinky (MCP, PIP, DIP, tip)
   
   FLEX_TO_SKELETON_MAP = {
       0: [1, 2, 3],    # Thumb flex → CMC, MCP, IP
       1: [5, 6, 7],    # Index flex → MCP, PIP, DIP
       2: [9, 10, 11],  # Middle flex → MCP, PIP, DIP
       3: [13, 14, 15], # Ring flex → MCP, PIP, DIP
       4: [17, 18, 19], # Pinky flex → MCP, PIP, DIP
   }
   
   KINEMATIC_RATIOS = {
       0: [0.5, 0.8, 0.7],  # Thumb: CMC, MCP, IP
       1: [1.0, 0.8, 0.6],  # Index: MCP, PIP, DIP
       2: [1.0, 0.8, 0.6],  # Middle
       3: [1.0, 0.8, 0.6],  # Ring
       4: [1.0, 0.8, 0.6],  # Pinky
   }
   
   class STGCNMapper:
       def __init__(self):
           self.num_nodes = 21
           self.num_features = 3  # x, y, z per node
       
       def map_to_skeleton(self, sensor_data: SensorData) -> np.ndarray:
           """Convert flex% + euler to 21-node skeleton coordinates."""
           skeleton = np.zeros((self.num_nodes, 3))
           
           # Wrist from IMU euler
           skeleton[0] = self._euler_to_wrist(sensor_data.euler)
           
           # Fingers from flex percentages
           for flex_idx, node_indices in FLEX_TO_SKELETON_MAP.items():
               flex_pct = sensor_data.finger_values[flex_idx] / 100.0
               ratios = KINEMATIC_RATIOS[flex_idx]
               
               for i, (node_idx, ratio) in enumerate(zip(node_indices, ratios)):
                   angle = flex_pct * ratio * np.pi / 2
                   skeleton[node_idx] = self._joint_position(
                       parent=skeleton[node_idx - 1 if i > 0 else 0],
                       angle=angle,
                       bone_length=self._get_bone_length(flex_idx, i)
                   )
           
           # Fingertips (nodes 4, 8, 12, 16, 20)
           for tip_idx in [4, 8, 12, 16, 20]:
               parent_idx = tip_idx - 1
               skeleton[tip_idx] = skeleton[parent_idx] + \
                   self._tip_offset(skeleton[parent_idx], skeleton[parent_idx - 1])
           
           return skeleton
   ```

2. Update `glove_relay/models/st_gcn_v4.py`:
   - Same architecture as V3 (280K params)
   - Retrain with V4 skeleton mappings
   - Input: (batch, 3, 30, 21) — 3D coordinates, 30 frames, 21 nodes

3. Create `glove_relay/models/train_st_gcn_v4.py`:
   - Load V4 dataset
   - Map sensor data to skeleton using STGCNMapper
   - Train ST-GCN model
   - Export to ONNX

[REQUIREMENTS]
- Same ST-GCN architecture as V3 (no structural changes)
- Only input mapping changes (flex% → skeleton coordinates)
- Support all 3 remapping schemes (A: direct, B: kinematic, C: hybrid)
- Default to Scheme C (hybrid)
- Output: gesture class probabilities

[TEST]
- Test: mapper produces valid skeleton coordinates for known flex values
- Test: skeleton visualization matches expected hand pose
- Test: ST-GCN model accepts mapped skeleton input
- Test: retrained model accuracy > 90% on validation set
```

---

## Prompt 12: Unity ms-MANO Update

```
[TASK] Update the Unity ms-MANO hand renderer for V4 flex sensor input.

[CONTEXT FROM PROMPT 00]
(Prepend Prompt 00 here)

[INPUT]
- V3: Hall values + quaternion → MANO θ parameters
- V4: Flex percentages + euler → MANO θ parameters
- Unity project: glove_unity/

[OUTPUT]
1. Update `glove_unity/Assets/Scripts/ManoMapper.cs`:

   ```csharp
   using UnityEngine;
   using System;
   
   public class ManoMapper : MonoBehaviour
   {
       [Header("MANO Model")]
       public SkinnedMeshRenderer handMesh;
       
       [Header("Kinematic Ratios")]
       public float[] thumbRatios = { 0.5f, 0.8f, 0.7f };
       public float[] fingerRatios = { 1.0f, 0.8f, 0.6f };
       
       // MANO θ parameters (hand pose)
       private float[] manoTheta = new float[45]; // 15 joints × 3 rotation axes
       
       public void UpdateHand(V4SensorData data)
       {
           // Map flex percentages to MANO θ
           MapFlexToMano(data.flexValues);
           
           // Map IMU euler to wrist rotation
           MapImuToWrist(data.euler);
           
           // Apply to hand mesh
           ApplyToMesh();
       }
       
       private void MapFlexToMano(float[] flexPercent)
       {
           // Thumb (3 joints)
           float thumbBase = flexPercent[0] / 100f * 90f;
           manoTheta[0] = thumbBase * thumbRatios[0]; // CMC
           manoTheta[3] = thumbBase * thumbRatios[1]; // MCP
           manoTheta[6] = thumbBase * thumbRatios[2]; // IP
           
           // Index (3 joints)
           float indexBase = flexPercent[1] / 100f * 90f;
           manoTheta[9] = indexBase * fingerRatios[0];  // MCP
           manoTheta[12] = indexBase * fingerRatios[1]; // PIP
           manoTheta[15] = indexBase * fingerRatios[2]; // DIP
           
           // Middle, Ring, Pinky (same pattern)
           for (int f = 2; f < 5; f++)
           {
               float baseAngle = flexPercent[f] / 100f * 90f;
               int offset = 9 + (f - 1) * 9;
               manoTheta[offset] = baseAngle * fingerRatios[0];
               manoTheta[offset + 3] = baseAngle * fingerRatios[1];
               manoTheta[offset + 6] = baseAngle * fingerRatios[2];
           }
       }
       
       private void MapImuToWrist(float[] euler)
       {
           // Apply IMU rotation to wrist bone
           transform.localRotation = Quaternion.Euler(
               euler[1],  // pitch
               euler[2],  // yaw
               euler[0]   // roll
           );
       }
       
       private void ApplyToMesh()
       {
           // Apply manoTheta to SkinnedMeshRenderer bone transforms
           // Implementation depends on MANO model rigging
           for (int i = 0; i < handMesh.bones.Length && i < 15; i++)
           {
               int thetaIdx = i * 3;
               handMesh.bones[i].localRotation = Quaternion.Euler(
                   manoTheta[thetaIdx],
                   manoTheta[thetaIdx + 1],
                   manoTheta[thetaIdx + 2]
               );
           }
       }
   }
   
   [Serializable]
   public struct V4SensorData
   {
       public float[] flexValues;  // 5 values, 0-100%
       public float[] euler;       // 3 values, degrees
       public float[] gyro;        // 3 values, °/s
   }
   ```

2. Update `glove_unity/Assets/Scripts/WebSocketClient.cs`:
   - Parse V4 JSON format
   - Extract `flex_values`, `imu_euler`
   - Pass to ManoMapper

[REQUIREMENTS]
- Smooth animation (lerp between frames)
- Flex 0% = open hand, 100% = closed fist
- IMU euler → wrist rotation
- Support both V3 and V4 data formats
- Configurable kinematic ratios per finger

[TEST]
- Test: flex 0% renders open hand
- Test: flex 100% renders closed fist
- Test: IMU rotation updates wrist correctly
- Test: smooth transitions between poses
```

---

## Prompt 13: End-to-End Integration Test

```
[TASK] Create end-to-end integration tests for the EchoGlove V4 pipeline.

[CONTEXT FROM PROMPT 00]
(Prepend Prompt 00 here)

[INPUT]
- Full pipeline: Sensors → Firmware → UDP → Relay → WebSocket → Frontend
- Test environment: local network, single glove, single relay server

[OUTPUT]
1. Create `glove_firmware/tests/test_e2e_v4.c`:
   ```c
   // Firmware-side integration tests
   
   // Test 1: Full sensor read cycle
   void test_sensor_read_cycle(void) {
       sensor_frame_t frame;
       esp_err_t err = sensor_manager_read_frame(&frame);
       TEST_ASSERT_EQUAL(ESP_OK, err);
       TEST_ASSERT_FLOAT_WITHIN(100.0f, 0.0f, frame.flex[0]);  // 0-100%
       TEST_ASSERT_FLOAT_WITHIN(180.0f, -180.0f, frame.euler[0]); // valid euler
   }
   
   // Test 2: Protobuf encode/decode round-trip
   void test_protobuf_roundtrip(void) {
       SensorFrame original = create_test_frame();
       uint8_t buffer[128];
       size_t len = protobuf_encode(&original, buffer, sizeof(buffer));
       
       SensorFrame decoded;
       protobuf_decode(buffer, len, &decoded);
       
       TEST_ASSERT_EQUAL(original.version, decoded.version);
       TEST_ASSERT_FLOAT_ARRAY_WITHIN(0.01f, original.flex_values, decoded.flex_values, 5);
   }
   
   // Test 3: UDP send (verify packet arrives)
   void test_udp_send(void) {
       // Send frame to known address
       // Verify reception on receiver side
   }
   ```

2. Create `glove_relay/tests/test_e2e_v4.py`:
   ```python
   import pytest
   import asyncio
   import websockets
   from glove_relay.protocol_parser import ProtocolParser
   from glove_relay.st_gcn_mapper import STGCNMapper
   
   class TestE2EV4:
       def test_full_pipeline(self):
           """Test: sensor data → parse → map → infer → output"""
           # 1. Create synthetic V4 packet
           packet = self.create_v4_packet()
           
           # 2. Parse
           parser = ProtocolParser()
           data = parser.parse(packet)
           assert data.version == 4
           assert len(data.finger_values) == 5
           assert len(data.euler) == 3
           
           # 3. Map to skeleton
           mapper = STGCNMapper()
           skeleton = mapper.map_to_skeleton(data)
           assert skeleton.shape == (21, 3)
           
           # 4. Verify no errors
           assert not np.any(np.isnan(skeleton))
       
       def test_v3_backward_compat(self):
           """Test: V3 packet still works"""
           packet = self.create_v3_packet()
           parser = ProtocolParser()
           data = parser.parse(packet)
           assert data.version == 3
           assert data.euler is not None  # Converted from quaternion
       
       def test_websocket_delivery(self):
           """Test: data arrives at WebSocket client"""
           # Start relay server
           # Connect WebSocket client
           # Send V4 UDP packet
           # Verify WebSocket message received
       
       def test_latency(self):
           """Test: end-to-end latency < 200ms"""
           # Send packet, measure time to WebSocket delivery
           # Assert < 200ms
   ```

3. Create `glove_web/src/__tests__/e2e.test.js`:
   - Test WebSocket connection
   - Test data parsing
   - Test hand skeleton rendering with mock data

[REQUIREMENTS]
- Tests must run without physical hardware (use mock/synthetic data)
- Test V3 backward compatibility
- Test V4 full pipeline
- Test latency < 200ms
- Test error handling (malformed packets, network issues)
- CI/CD compatible (run on GitHub Actions)

[TEST]
- All tests pass with `pytest` (Python) and `pio test` (firmware)
- No hardware required for unit/integration tests
- Latency test passes consistently
```

---

## Prompt 14: Documentation & Release

```
[TASK] Create V4 documentation and prepare release.

[CONTEXT FROM PROMPT 00]
(Prepend Prompt 00 here)

[INPUT]
- All V4 code changes complete
- Tests passing
- Models trained and exported

[OUTPUT]
1. Create `docs/V4_MIGRATION_GUIDE.md`:
   - Summary of changes from V3 to V4
   - Hardware changes (what to remove, what to add)
   - Wiring diagram
   - Firmware update instructions
   - Calibration procedure
   - Troubleshooting

2. Update `README.md`:
   - Update hardware requirements
   - Update wiring diagram
   - Update build instructions
   - Add V4 badge/section

3. Create `docs/CALIBRATION_GUIDE.md`:
   - Step-by-step calibration procedure
   - Calibration button usage
   - NVS persistence explanation
   - Recalibration triggers

4. Create `docs/DATASET_GUIDE.md`:
   - How to use the dataset collection tool
   - Gesture list and reference images
   - Data format specification
   - Export options

5. Create `CHANGELOG.md` entry:
   ```markdown
   ## [4.0.0] - 2026-XX-XX
   
   ### Changed
   - Migrated from Hall-effect to flex sensor architecture
   - BNO085 now uses 6DOF Game Rotation Vector (no magnetometer)
   - Feature vector: 11-dim (5 flex + 3 euler + 3 gyro) instead of 15-dim
   - L1 model input: 330-dim instead of 450-dim
   - Protobuf schema V4 (backward compatible with V3)
   
   ### Added
   - ADS1115 16-bit ADC driver (dual instance)
   - Flex sensor manager with NVS calibration
   - Dataset collection web tool
   - V3/V4 auto-detection in relay server
   
   ### Removed
   - TMAG5273 Hall-effect sensor driver
   - TCA9548A I2C MUX driver
   - N52 neodymium magnets (no longer needed)
   - Magnetometer data from IMU pipeline
   
   ### Fixed
   - Magnetic cross-talk between adjacent fingers
   - BNO085 magnetometer saturation from fingertip magnets
   - Temperature-dependent magnet drift
   ```

6. Create `docs/ARCHITECTURE.md`:
   - System architecture diagrams (Mermaid)
   - Data flow diagrams
   - I2C bus topology
   - Feature vector specification

[REQUIREMENTS]
- All docs in Markdown format
- Include Mermaid diagrams
- Reference actual file paths in project structure
- Include troubleshooting section
- Version tag: v4.0.0

[TEST]
- All documentation links are valid
- Mermaid diagrams render correctly
- Build instructions work from clean checkout
- Calibration guide matches actual firmware behavior
```

---

*End of Claude Code Prompts V4*
