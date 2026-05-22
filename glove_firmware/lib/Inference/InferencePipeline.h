/* =============================================================================
 * InferencePipeline — Glue between SlidingWindow, BaseModel, InferenceTrigger
 * =============================================================================
 * runInferencePipeline():
 *   1. Check model ready + window full
 *   2. Reconstruct SensorData frames from SlidingWindow float buffer
 *   3. model.preprocess() → model.infer() → model.postprocess()
 *   4. InferenceTrigger.update() for debouncing
 *   5. Populates GestureResult and returns confirmed status
 * =============================================================================
 */
#ifndef INFERENCE_PIPELINE_H
#define INFERENCE_PIPELINE_H

#include "data_structures.h"
#include "SlidingWindow.h"
#include "BaseModel.h"
#include "InferenceTrigger.h"
#include <cstring>

inline bool runInferencePipeline(
    SlidingWindow& window,
    BaseModel& model,
    InferenceTrigger& trigger,
    GestureResult& result,
    uint32_t timestamp_us = 0)
{
    result.zero();

    // Preconditions
    if (!model.isReady() || !window.isFull()) {
        return false;
    }

    ModelInfo info = model.get_model_info();
    size_t input_size = (size_t)info.window_size * info.input_features;
    size_t output_size = info.num_classes;

    // Allocate working buffers (heap for native, PSRAM for ESP32 in BaseModel::run)
    float* input_buf = new float[input_size];
    float* output_buf = new float[output_size];

    if (!input_buf || !output_buf) {
        delete[] input_buf;
        delete[] output_buf;
        return false;
    }

    // Reconstruct SensorData frames from contiguous float buffer
    const float* raw = window.getBuffer();
    SensorData* frames = new SensorData[info.window_size];
    for (int i = 0; i < info.window_size; i++) {
        frames[i].zero();
        memcpy(frames[i].hall_xyz, raw + i * FEATURE_COUNT,
               HALL_FEATURE_COUNT * sizeof(float));
        memcpy(frames[i].euler,
               raw + i * FEATURE_COUNT + HALL_FEATURE_COUNT,
               3 * sizeof(float));
        memcpy(frames[i].gyro,
               raw + i * FEATURE_COUNT + HALL_FEATURE_COUNT + 3,
               3 * sizeof(float));
    }

    // Pipeline: preprocess → infer → postprocess
    int ret = model.preprocess(input_buf, frames, info.window_size);
    if (ret < 0) {
        delete[] input_buf;
        delete[] output_buf;
        delete[] frames;
        return false;
    }

    ret = model.infer(input_buf, output_buf);
    if (ret < 0) {
        delete[] input_buf;
        delete[] output_buf;
        delete[] frames;
        return false;
    }

    ret = model.postprocess(output_buf, &result);

    delete[] input_buf;
    delete[] output_buf;
    delete[] frames;

    if (ret < 0) {
        return false;
    }

    // Debounce via InferenceTrigger
    bool confirmed = trigger.update(result.confidence, result.gesture_id, timestamp_us);

    return confirmed;
}

#endif
