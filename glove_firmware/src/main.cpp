/* =============================================================================
 * EdgeAI Data Glove V3 — Main Entry Point
 * =============================================================================
 * FreeRTOS dual-core task architecture for ESP32-S3:
 *
 *   Core 1 (Protocol CPU):
 *     └─ Task_SensorRead  (Priority 3, 100 Hz)
 *        - Reads all sensors via I2C (TCA9548A mux + 5× TMAG5273 + BNO085)
 *        - Kalman filtering is done inside SensorManager.readAll()
 *        - Normalizes features via FeatureNormalizer
 *        - Pushes normalized features into SlidingWindow ring buffer
 *        - Sends SensorData to FreeRTOS data queue
 *        - Outputs CSV via serial (for Edge Impulse data forwarder)
 *
 *   Core 0 (Application CPU):
 *     ├─ Task_Inference   (Priority 2, ~30 Hz)
 *     │  - Collects WINDOW_SIZE frames from SlidingWindow
 *     │  - Runs L1 inference via ModelRegistry
 *     │  - Pushes InferenceResult → inferenceQueue
 *     │
 *     └─ Task_Comms       (Priority 1, 100 Hz send / 20 Hz BLE)
 *        - Manages BLE provisioning (NimBLE)
 *        - Sends FullDataPacket over UDP (Nanopb serialized)
 *        - Falls back to BLE if WiFi unavailable
 *        - Handles incoming UDP commands (model switch, calibration)
 *
 * CRITICAL BUG FIX (from V2):
 *   V2 had WRONG xTaskCreatePinnedToCore parameter order — it passed the
 *   TaskHandle_t variable name as the function pointer:
 *
 *     WRONG: xTaskCreatePinnedToCore(TaskSensorReadHandle, "SensorRead", ...)
 *            ^^^^^^^^^^^^^^ This is a TaskHandle_t, NOT a function pointer!
 *            → Crash at boot or undefined behavior
 *
 *   V3 uses the CORRECT order with static_assert validation:
 *
 *     CORRECT: xTaskCreatePinnedToCore(Task_SensorRead, "SensorRead",
 *               10000, NULL, 3, &TaskSensorReadHandle, 1)
 *               ^^^^^^^^^^^^^^ This is a TaskFunction_t (void (*)(void*))
 *
 *   Parameter order: (function_ptr, name, stack_size, param, priority, handle_ptr, core_id)
 *
 * Signal Processing Pipeline (Phase 2):
 *   SensorData → toFeatureArray() → FeatureNormalizer.updateStats/normalize()
 *               → SlidingWindow.push() → [ready for inference when isFull()]
 *
 * Build: pio run -e esp32-s3-devkitc-1-n16r8
 * Monitor: pio device monitor
 * =============================================================================
 */

#include <Arduino.h>

// ---- FreeRTOS Headers ----
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

// ---- ESP-IDF Headers ----
#include "esp_timer.h"
#include "esp_system.h"
#include "esp_log.h"
#ifdef BOARD_HAS_PSRAM
#include "esp32-hal-psram.h"
#endif

// ---- Project Headers ----
#include "data_structures.h"

// ---- Sensor Libraries ----
#include "SensorManager.h"
#include "FlexManager.h"

// ---- Signal Processing Libraries ----
#include "SlidingWindow.h"
#include "FeatureNormalizer.h"

// ---- Communication Libraries ----
// #include "lib/Comms/BLEManager.h"
// #include "lib/Comms/UDPTransmitter.h"

// ---- Model Libraries ----
#include "ModelRegistry.h"
#include "InferenceTrigger.h"
#include "InferencePipeline.h"

// =============================================================================
// Global Objects
// =============================================================================

/// Unified sensor manager (I2C bus, mux, Hall sensors, IMU, Kalman filters)
static SensorManager g_sensor_manager;

/// Flex sensor manager (ADC, currently placeholder)
static FlexManager g_flex_manager;

/// Sliding window ring buffer (30 frames × 21 features, allocated in PSRAM)
static SlidingWindow g_sliding_window;

/// Feature normalizer (Min-Max, 2-second calibration then normalize)
static FeatureNormalizer g_normalizer;

/// BLE provisioning + data notification manager
// static BLEManager g_ble_manager;

/// WiFi UDP transmitter
// static UDPTransmitter g_udp_transmitter;

/// Model registry for L1 inference + hot-switching
static ModelRegistry& g_model_registry = ModelRegistry::instance();

/// Inference trigger (confidence gating + debouncing)
static InferenceTrigger g_inference_trigger;

// =============================================================================
// FreeRTOS Queues
// =============================================================================

/// Queue for sensor data: SensorRead → Inference + Comms
static QueueHandle_t g_data_queue = nullptr;

/// Queue for inference results: Inference → Comms
static QueueHandle_t g_inference_queue = nullptr;

// =============================================================================
// Task Handles (for static_assert validation)
// =============================================================================

static TaskHandle_t TaskSensorReadHandle = nullptr;
static TaskHandle_t TaskInferenceHandle = nullptr;
static TaskHandle_t TaskCommsHandle = nullptr;

// =============================================================================
// Task Function Prototypes
// =============================================================================

void Task_SensorRead(void* pvParameters);
void Task_Inference(void* pvParameters);
void Task_Comms(void* pvParameters);

// =============================================================================
// Static Asserts (compile-time verification of function pointers)
// =============================================================================

static_assert(std::is_same<decltype(&Task_SensorRead), TaskFunction_t>::value,
              "Task_SensorRead must have signature void(void*)");
static_assert(std::is_same<decltype(&Task_Inference), TaskFunction_t>::value,
              "Task_Inference must have signature void(void*)");
static_assert(std::is_same<decltype(&Task_Comms), TaskFunction_t>::value,
              "Task_Comms must have signature void(void*)");

// =============================================================================
// Calibration Constants
// =============================================================================

/// Calibration duration in frames (2 seconds @ 100Hz)
static constexpr uint32_t CALIBRATION_FRAMES = 200;

/// Serial CSV output period (every N frames, for Edge Impulse data forwarder)
static constexpr uint32_t CSV_OUTPUT_PERIOD = 1;  // Every frame for data collection

// =============================================================================
// Setup
// =============================================================================

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println("\n========================================");
    Serial.println("EdgeAI Data Glove V3");
    Serial.println("ESP32-S3-DevKitC-1 N16R8");
    Serial.println("Phase 1+2: HAL + Signal Processing");
    Serial.println("========================================\n");

    // Check PSRAM availability
    Serial.printf("Total heap: %d bytes\n", ESP.getHeapSize());
    Serial.printf("Free heap: %d bytes\n", ESP.getFreeHeap());

    if (psramFound()) {
        Serial.printf("PSRAM found: %d bytes\n", ESP.getPsramSize());
        Serial.printf("Free PSRAM: %d bytes\n", ESP.getFreePsram());
    } else {
        Serial.println("WARNING: PSRAM not found!");
    }

    // Initialize feature normalizer (starts in calibration mode)
    g_normalizer.reset();
    Serial.println("[setup] Feature normalizer initialized (calibration mode)");

    // Create queues
    g_data_queue = xQueueCreate(10, sizeof(SensorData));
    g_inference_queue = xQueueCreate(10, sizeof(InferenceResult));

    if (g_data_queue == nullptr || g_inference_queue == nullptr) {
        Serial.println("ERROR: Failed to create queues");
        return;
    }

    // Create FreeRTOS tasks (pinned to specific cores)

    // Task_SensorRead: Core 1, Priority 3, 100Hz
    xTaskCreatePinnedToCore(
        Task_SensorRead,
        "SensorRead",
        10000,
        nullptr,
        3,
        &TaskSensorReadHandle,
        1
    );

    // Task_Inference: Core 0, Priority 2, ~30Hz
    xTaskCreatePinnedToCore(
        Task_Inference,
        "Inference",
        16384,
        nullptr,
        2,
        &TaskInferenceHandle,
        0
    );

    // Task_Comms: Core 0, Priority 1, 100Hz send / 20Hz BLE
    xTaskCreatePinnedToCore(
        Task_Comms,
        "Comms",
        8192,
        nullptr,
        1,
        &TaskCommsHandle,
        0
    );

    Serial.println("FreeRTOS tasks created successfully");
    Serial.println("System ready\n");
}

// =============================================================================
// Main Loop (empty - all work in tasks)
// =============================================================================

void loop() {
    vTaskDelay(pdMS_TO_TICKS(1000));
}

// =============================================================================
// Task_SensorRead — Full signal processing pipeline
// =============================================================================

void Task_SensorRead(void* pvParameters) {
    Serial.println("[Task_SensorRead] Started on Core 1");

    // Initialize sensors (SensorManager handles I2C, mux, Hall, IMU, Kalman)
    if (!g_sensor_manager.begin()) {
        Serial.println("[Task_SensorRead] FATAL: Sensor init failed!");
        while (true) vTaskDelay(pdMS_TO_TICKS(1000));
    }

    // Initialize flex sensor manager (placeholder, returns zeros)
    g_flex_manager.begin();

    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(10);  // 100 Hz

    uint32_t frame_count = 0;
    uint32_t calibration_count = 0;
    bool calibration_complete = false;

    Serial.println("[Task_SensorRead] Calibration: collecting stats for 2 seconds...");

    while (true) {
        SensorData data;
        data.seq = frame_count;

        // ---- Step 1: Read all sensors (Kalman filtering is done inside) ----
        if (!g_sensor_manager.readAll(data)) {
            frame_count++;
            vTaskDelayUntil(&xLastWakeTime, xFrequency);
            continue;
        }

        // ---- Step 2: Read flex sensors (placeholder: zeros) ----
        g_flex_manager.readAll(data.flex);

        // ---- Step 3: Extract 21-dim feature vector ----
        float features[FEATURE_COUNT];
        data.toFeatureArray(features);

        // ---- Step 4: Calibration phase (first 2 seconds) ----
        if (!calibration_complete) {
            g_normalizer.updateStats(features);
            calibration_count++;

            if (calibration_count >= CALIBRATION_FRAMES) {
                calibration_complete = true;
                Serial.println("[Task_SensorRead] Calibration complete. Feature ranges:");
                for (uint8_t i = 0; i < FEATURE_COUNT; i++) {
                    Serial.printf("  [%2d] min=%8.3f max=%8.3f range=%8.3f\n",
                                 i, g_normalizer.getMin(i),
                                 g_normalizer.getMax(i), g_normalizer.getRange(i));
                }
            }
        }

        // ---- Step 5: Normalize features to [0, 1] ----
        if (calibration_complete) {
            g_normalizer.normalize(features);
        }

        // ---- Step 6: Push normalized features into SlidingWindow ----
        g_sliding_window.push(features);

        // ---- Step 7: Send SensorData to FreeRTOS queue ----
        if (xQueueSend(g_data_queue, &data, 0) != pdPASS) {
            Serial.printf("[Task_SensorRead] Queue full, dropped frame %lu\n", frame_count);
        }

        // ---- Step 8: Serial CSV output (for Edge Impulse data forwarder) ----
        // Format: timestamp, hall0x, hall0y, hall0z, ..., hall4z, roll, pitch, yaw, gx, gy, gz
        if (frame_count % CSV_OUTPUT_PERIOD == 0 && calibration_complete) {
            Serial.printf("%lu", data.timestamp_us);
            for (uint8_t i = 0; i < HALL_FEATURE_COUNT; i++) {
                Serial.printf(",%.4f", data.hall_xyz[i]);
            }
            Serial.printf(",%.2f,%.2f,%.2f", data.euler[0], data.euler[1], data.euler[2]);
            Serial.printf(",%.2f,%.2f,%.2f", data.gyro[0], data.gyro[1], data.gyro[2]);
            Serial.println();
        }

        // ---- Step 9: Debug output (every 100 frames) ----
        if (frame_count % 100 == 0) {
            Serial.printf("[Task_SensorRead] Frame %lu | Window: %d/%d | "
                         "Free heap: %lu | Cal: %s\n",
                         frame_count, g_sliding_window.getFrameCount(), WINDOW_SIZE,
                         ESP.getFreeHeap(),
                         calibration_complete ? "DONE" : "RUNNING");
        }

        frame_count++;
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
}

// =============================================================================
// Task_Inference — L1 Edge Inference (Phase 3)
// =============================================================================

void Task_Inference(void* pvParameters) {
    Serial.println("[Task_Inference] Started on Core 0");

    while (true) {
        BaseModel* active_model = g_model_registry.getActiveModel();
        if (active_model && g_sliding_window.isFull()) {
            GestureResult result;
            result.zero();

            uint32_t now_us = (uint32_t)esp_timer_get_time();
            bool confirmed = runInferencePipeline(
                g_sliding_window, *active_model,
                g_inference_trigger, result, now_us);

            if (confirmed) {
                InferenceResult ir;
                ir.gesture = result;
                ir.timestamp_us = now_us;
                strncpy(ir.model_name, active_model->name(), sizeof(ir.model_name) - 1);

                xQueueSend(g_inference_queue, &ir, 0);

                Serial.printf("[Inference] Gesture %d conf=%.2f (%s)\n",
                              result.gesture_id, result.confidence,
                              active_model->name());
            }
        }

        vTaskDelay(pdMS_TO_TICKS(33));  // ~30 Hz
    }
}

// =============================================================================
// Task_Comms — Placeholder (Phase 4 will implement BLE/UDP)
// =============================================================================

void Task_Comms(void* pvParameters) {
    Serial.println("[Task_Comms] Started on Core 0 (placeholder)");

    while (true) {
        // Phase 4: Will implement BLE provisioning + UDP telemetry here
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}