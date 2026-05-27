/* =============================================================================
 * EdgeAI Data Glove V3 — Shared Data Structures & Constants
 * =============================================================================
 * Single header included by all firmware modules.  Defines the canonical
 * packet layout, model metadata structs, and compile-time constants.
 *
 * V3 Changes from V2:
 *   - Unified FullDataPacket (sensors + flex + inference)
 *   - GestureResult carries confidence + metadata for L1/L2 handoff
 *   - Compile-time FEATURE_COUNT = 21 (15 hall + 6 IMU)
 *   - Fixed FreeRTOS xTaskCreatePinnedToCore parameter order bug
 * =============================================================================
 */

#ifndef DATA_STRUCTURES_H
#define DATA_STRUCTURES_H

#include <cstdint>
#include <cstring>
#ifndef UNIT_TEST
#include <Arduino.h>
#else
// Stubs for native testing
#ifndef PROGMEM
#define PROGMEM
#endif
#include <cstdio>
#include <cstdlib>
#include <cstdarg>
#define ps_malloc malloc
// Minimal Serial stub
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

// =============================================================================
// Compile-Time Constants
// =============================================================================

/// Number of 3-axis Hall sensors on the glove
/// TODO: Restore to 5 when all fingers are wired (ch0-ch4)
static constexpr uint8_t NUM_HALL_SENSORS = 1;  // Only thumb (ch0) connected

/// Axes per Hall sensor (X, Y, Z)
static constexpr uint8_t HALL_AXES = 3;

/// Total Hall feature count (5 sensors × 3 axes)
static constexpr uint8_t HALL_FEATURE_COUNT = NUM_HALL_SENSORS * HALL_AXES;  // 15

/// IMU feature count (3 euler + 3 gyro)
static constexpr uint8_t IMU_FEATURE_COUNT = 6;

/// Total sensor features per frame
static constexpr uint8_t FEATURE_COUNT = HALL_FEATURE_COUNT + IMU_FEATURE_COUNT;  // 21

/// Number of flex sensors (reserved, ADC not yet populated)
static constexpr uint8_t NUM_FLEX_SENSORS = 5;

/// Sliding window length for temporal inference (frames)
static constexpr uint8_t WINDOW_SIZE = 30;

/// Total gesture classes (ASL alphabet 26 + digits 10 + space + del + etc.)
static constexpr uint8_t NUM_CLASSES = 46;

/// Target sensor sampling rate (Hz)
static constexpr uint16_t SENSOR_RATE_HZ = 100;

/// UDP telemetry port
static constexpr uint16_t UDP_PORT = 8888;

/// FreeRTOS queue depths
static constexpr uint8_t DATA_QUEUE_DEPTH = 10;
static constexpr uint8_t INFERENCE_QUEUE_DEPTH = 10;

/// Ring buffer size for sensor data (≥ WINDOW_SIZE + headroom)
static constexpr uint16_t SENSOR_RING_SIZE = 64;

// =============================================================================
// SensorData — Raw sensor readings for a single sample tick
// =============================================================================

struct SensorData {
    /// 3D Hall sensor readings [sensor_idx][axis] — millitesla (mT)
    float hall_xyz[HALL_FEATURE_COUNT];       // 15 floats

    /// Orientation quaternion [w, x, y, z] from BNO085
    float quaternion[4];

    /// Euler angles [roll, pitch, yaw] in degrees
    float euler[3];

    /// Angular velocity [x, y, z] in deg/s
    float gyro[3];

    /// Flex sensor ADC readings (normalized 0.0–1.0), currently zeros
    float flex[NUM_FLEX_SENSORS];

    /// Monotonic timestamp in microseconds (from esp_timer_get_time())
    uint32_t timestamp_us;

    /// Sequence counter for drop detection
    uint32_t seq;

    // ---- Helpers ----

    /** Copy hall + IMU features into a flat float array of size FEATURE_COUNT */
    void toFeatureArray(float out[FEATURE_COUNT]) const {
        memcpy(out, hall_xyz, HALL_FEATURE_COUNT * sizeof(float));
        memcpy(out + HALL_FEATURE_COUNT, euler, 3 * sizeof(float));
        memcpy(out + HALL_FEATURE_COUNT + 3, gyro, 3 * sizeof(float));
    }

    void zero() {
        memset(this, 0, sizeof(SensorData));
    }
};

// Static assertion: SensorData must be a POD-like struct for queue safety
static_assert(std::is_trivially_copyable<SensorData>::value,
    "SensorData must be trivially copyable for FreeRTOS queues");

// =============================================================================
// GestureResult — L1 inference output
// =============================================================================

struct GestureResult {
    /// Predicted class index (0 .. NUM_CLASSES-1)
    int32_t gesture_id = -1;

    /// Softmax confidence for the top prediction
    float confidence = 0.0f;

    /// Per-class softmax scores (optional, filled when debug enabled)
    float scores[NUM_CLASSES] = {};

    /// True if confidence exceeds threshold and result should be emitted
    bool valid = false;

    /// True if L2 (cloud/edge) inference is requested for this gesture
    bool l2_requested = false;

    void zero() {
        memset(this, 0, sizeof(GestureResult));
        gesture_id = -1;
    }
};

static_assert(std::is_trivially_copyable<GestureResult>::value,
    "GestureResult must be trivially copyable for FreeRTOS queues");

// =============================================================================
// ModelInfo — Runtime model metadata
// =============================================================================

struct ModelInfo {
    /// Human-readable model name (max 31 chars + null)
    char name[32] = {};

    /// Model type string: "CNNAttention", "MSTCN", etc.
    char type[16] = {};

    /// Expected input feature count per frame
    uint16_t input_features = 0;

    /// Expected temporal window length (1 for frame-wise models)
    uint16_t window_size = 1;

    /// Number of output classes
    uint16_t num_classes = 0;

    /// Model binary size in bytes
    uint32_t model_size_bytes = 0;

    /// Arena memory used by TFLite interpreter
    uint32_t arena_size_bytes = 0;
};

// =============================================================================
// FullDataPacket — Wire format for sensor + inference (enqueued for Comms)
// =============================================================================

struct FullDataPacket {
    /// Latest sensor frame
    SensorData sensor;

    /// L1 inference result (zeroed if not yet inferred)
    GestureResult inference;

    /// System status flag
    enum Status : uint8_t {
        STREAMING = 0,
        MODEL_SWITCHING = 1,
        ERROR_STATE = 2,
        CALIBRATING = 3,
        IDLE = 4
    } status = STREAMING;

    /// Pack sensor features into contiguous float array for model input
    void toFeatureArray(float out[FEATURE_COUNT]) const {
        sensor.toFeatureArray(out);
    }
};

static_assert(std::is_trivially_copyable<FullDataPacket>::value,
    "FullDataPacket must be trivially copyable for FreeRTOS queues");

// =============================================================================
// InferenceResult — Lighter packet pushed onto inferenceQueue
// =============================================================================

struct InferenceResult {
    GestureResult gesture;
    uint32_t timestamp_us = 0;
    char model_name[32] = {};
};

static_assert(std::is_trivially_copyable<InferenceResult>::value,
    "InferenceResult must be trivially copyable for FreeRTOS queues");

// =============================================================================
// I2C Pin Assignments (ESP32-S3 specific GPIO numbers)
// =============================================================================

namespace I2CPins {
    static constexpr uint8_t SDA = 8;   // Grove I2C data
    static constexpr uint8_t SCL = 9;   // Grove I2C clock
    static constexpr uint32_t FREQ = 400000;  // 400 kHz I2C
}

// =============================================================================
// Mux Channel Assignments (TCA9548A)
// =============================================================================

namespace MuxChannels {
    static constexpr uint8_t HALL_SENSOR_0 = 0;   // Thumb
    static constexpr uint8_t HALL_SENSOR_1 = 1;   // Index
    static constexpr uint8_t HALL_SENSOR_2 = 2;   // Middle
    static constexpr uint8_t HALL_SENSOR_3 = 3;   // Ring
    static constexpr uint8_t HALL_SENSOR_4 = 4;   // Pinky
    static constexpr uint8_t BNO085_IMU     = 5;  // Dedicated IMU bus (SD5/SC5)
}

// =============================================================================
// Flex Sensor ADC Pin Assignments (reserved for V3.1)
// =============================================================================

namespace FlexPins {
    static constexpr uint8_t FLEX_0 = 4;   // Thumb
    static constexpr uint8_t FLEX_1 = 5;   // Index
    static constexpr uint8_t FLEX_2 = 6;   // Middle
    static constexpr uint8_t FLEX_3 = 7;   // Ring
    static constexpr uint8_t FLEX_4 = 15;  // Pinky
}

#endif // DATA_STRUCTURES_H
