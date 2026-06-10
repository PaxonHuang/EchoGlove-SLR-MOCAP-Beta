#pragma once

/**
 * @file tflite_infer.h
 * @brief TFLite Micro inference wrapper for Tier2 GatedBiCrossAttention.
 *
 * The Tier2 model takes TWO 11-dim inputs (left hand, right hand) and
 * produces a 46-class probability vector.  The 6 relative features are
 * NOT used by this model.
 *
 * Input layout per hand:
 *   [0..4]  flex sensors (5)
 *   [5..7]  euler angles (3)
 *   [8..10] gyro (3)
 */

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TFLITE_INPUT_DIM    11
#define TFLITE_NUM_CLASSES  46

/** Inference result from Tier2 model. */
typedef struct {
    int gesture_id;                         /**< Predicted class index (-1 = invalid) */
    float confidence;                       /**< Softmax probability of top class */
    float probabilities[TFLITE_NUM_CLASSES]; /**< Full softmax output */
    uint32_t inference_us;                  /**< Inference time in microseconds */
    bool valid;                             /**< True if inference succeeded */
} Tier2Result;

/**
 * @brief Initialize the TFLite Micro interpreter.
 *
 * Loads the model, allocates the arena in PSRAM, and sets up the
 * interpreter.  Must be called once before tflite_run().
 *
 * @param model_data  Pointer to .tflite model binary (model_data.h).
 * @param model_len   Length in bytes.
 * @return true on success.
 */
bool tflite_init(const unsigned char* model_data, unsigned int model_len);

/**
 * @brief Run a single inference on a left/right hand pair.
 *
 * @param left   11-dim float array for left hand features.
 * @param right  11-dim float array for right hand features.
 * @return Tier2Result with prediction, confidence, and timing.
 */
Tier2Result tflite_run(const float left[TFLITE_INPUT_DIM],
                       const float right[TFLITE_INPUT_DIM]);

/**
 * @brief Return current arena usage in bytes.
 *
 * @return Arena bytes used, or 0 if not initialized.
 */
uint32_t tflite_arena_usage(void);

#ifdef __cplusplus
}
#endif
