/* =============================================================================
 * EchoGlove V5 — Shared Data Structures & Constants
 * =============================================================================
 * Single header included by all firmware modules. Defines the canonical
 * packet layout, model metadata structs, and compile-time constants.
 *
 * V5 Changes from V3:
 *   - Hall sensors removed; flex sensors (ADS1115) replace them
 *   - DUAL_HAND_FEATURES = 28 (2 x 11 flex+IMU + 6 cross-hand)
 *   - GlovePacket for ESP-NOW (69 bytes, CRC-16/MODBUS)
 *   - I2C freq lowered to 100 kHz for 3 devices on bus
 *   - MuxChannels and FlexPins namespaces removed
 * =============================================================================
 */

#ifndef DATA_STRUCTURES_H
#define DATA_STRUCTURES_H

#include <cstdint>
#include <cstring>
#ifndef UNIT_TEST
#include <Arduino.h>
#else
#ifndef PROGMEM
#define PROGMEM
#endif
#include <cstdio>
#include <cstdlib>
#include <cstdarg>
#define ps_malloc malloc
struct _SerialStub {
    void println(const char* s) { puts(s); }
    void printf(const char* fmt, ...) {
        va_list args;
        va_start(args, fmt);
        vprintf(fmt, args);
        va_end(args);
    }
};
static _SerialStub Serial;
#endif

// ── V5.0 Constants ──────────────────────────────────────────────
#define NUM_FLEX_SENSORS      5
#define IMU_FEATURE_COUNT     6      // 3 euler + 3 gyro
#define SINGLE_HAND_FEATURES  (NUM_FLEX_SENSORS + IMU_FEATURE_COUNT)  // 11
#define DUAL_HAND_FEATURES    (2 * SINGLE_HAND_FEATURES + 6)          // 28
#define WINDOW_SIZE           30
#define NUM_CLASSES           46
#define SENSOR_RATE_HZ        100
#define CALIBRATION_DURATION_MS  6000  // 3s open + 3s fist

// ── Backward-compatibility aliases (V3 → V5 migration) ──────────
#define FEATURE_COUNT         SINGLE_HAND_FEATURES   // was 21, now 11
#define HALL_FEATURE_COUNT    NUM_FLEX_SENSORS        // was 6, now 5

// FreeRTOS queue depths
#define DATA_QUEUE_DEPTH      10
#define INFERENCE_QUEUE_DEPTH 10
#define SENSOR_RING_SIZE      64

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

// ── I2C Pins (ESP32-S3) ────────────────────────────────────────
namespace I2CPins {
    static constexpr uint8_t SDA = 8;
    static constexpr uint8_t SCL = 9;
    static constexpr uint32_t FREQ = 100000;  // 100kHz for 3 devices
}

// ── SensorData ──────────────────────────────────────────────────
struct SensorData {
    union {
        float flex[NUM_FLEX_SENSORS];     // 5 normalized [0,1] (V5)
        float hall_xyz[NUM_FLEX_SENSORS]; // backward-compat alias (V3)
    };
    float quaternion[4];               // BNO085 (w,x,y,z)
    float euler[3];                    // roll, pitch, yaw (degrees)
    float gyro[3];                     // angular velocity (deg/s)
    uint32_t timestamp_us;
    uint32_t seq;

    void toFeatureArray(float* out) const {
        memcpy(out, flex, NUM_FLEX_SENSORS * sizeof(float));
        memcpy(out + NUM_FLEX_SENSORS, euler, 3 * sizeof(float));
        memcpy(out + NUM_FLEX_SENSORS + 3, gyro, 3 * sizeof(float));
    }

    void zero() { memset(this, 0, sizeof(SensorData)); }
};

static_assert(std::is_trivially_copyable<SensorData>::value,
    "SensorData must be trivially copyable for FreeRTOS queues");

// ── GestureResult ───────────────────────────────────────────────
struct GestureResult {
    int32_t gesture_id = -1;
    float confidence = 0.0f;
    float scores[NUM_CLASSES] = {};
    bool valid = false;
    bool l2_requested = false;
    void zero() {
        memset(this, 0, sizeof(GestureResult));
        gesture_id = -1;
    }
};

static_assert(std::is_trivially_copyable<GestureResult>::value,
    "GestureResult must be trivially copyable for FreeRTOS queues");

// ── ModelInfo ───────────────────────────────────────────────────
struct ModelInfo {
    char name[32] = {};
    char type[16] = {};
    uint16_t input_features = 0;
    uint16_t window_size = 1;
    uint16_t num_classes = 0;
    uint32_t model_size_bytes = 0;
    uint32_t arena_size_bytes = 0;
};

// ── FullDataPacket ──────────────────────────────────────────────
struct FullDataPacket {
    SensorData sensor;
    GestureResult inference;
    enum Status : uint8_t {
        STREAMING = 0,
        MODEL_SWITCHING = 1,
        ERROR_STATE = 2,
        CALIBRATING = 3,
        IDLE = 4
    } status = STREAMING;
    void toFeatureArray(float out[SINGLE_HAND_FEATURES]) const {
        sensor.toFeatureArray(out);
    }
};

static_assert(std::is_trivially_copyable<FullDataPacket>::value,
    "FullDataPacket must be trivially copyable for FreeRTOS queues");

// ── InferenceResult ─────────────────────────────────────────────
struct InferenceResult {
    GestureResult gesture;
    uint32_t timestamp_us = 0;
    char model_name[32] = {};
};

static_assert(std::is_trivially_copyable<InferenceResult>::value,
    "InferenceResult must be trivially copyable for FreeRTOS queues");

// ── GlovePacket (ESP-NOW, 69 bytes) ─────────────────────────────
#pragma pack(push, 1)
struct __attribute__((packed)) GlovePacket {
    uint8_t  magic[2];            // {0x45, 0x47} = "EG"
    uint8_t  version;             // 5
    uint8_t  hand_id;             // 0=left, 1=right
    uint32_t tick_id;
    uint32_t timestamp_us;
    float    flex[5];             // 20 bytes
    float    imu[6];              // 24 bytes (euler[3] + gyro[3])
    uint16_t l1_gesture_id;
    float    l1_confidence;
    uint8_t  status;
    uint8_t  reserved[4];         // 4 bytes padding (future use)
    uint16_t checksum;            // CRC-16/MODBUS over bytes 0–66

    void computeChecksum() {
        uint16_t crc = 0xFFFF;
        const uint8_t* data = reinterpret_cast<const uint8_t*>(this);
        for (size_t i = 0; i < 67; i++) {
            crc ^= data[i];
            for (int j = 0; j < 8; j++)
                crc = (crc & 1) ? (crc >> 1) ^ 0xA001 : crc >> 1;
        }
        checksum = crc;
    }

    bool verifyChecksum() const {
        uint16_t crc = 0xFFFF;
        const uint8_t* data = reinterpret_cast<const uint8_t*>(this);
        for (size_t i = 0; i < 67; i++) {
            crc ^= data[i];
            for (int j = 0; j < 8; j++)
                crc = (crc & 1) ? (crc >> 1) ^ 0xA001 : crc >> 1;
        }
        return checksum == crc;
    }
};
#pragma pack(pop)

static_assert(sizeof(GlovePacket) == 69,
    "GlovePacket must be exactly 69 bytes for ESP-NOW");

#endif // DATA_STRUCTURES_H
