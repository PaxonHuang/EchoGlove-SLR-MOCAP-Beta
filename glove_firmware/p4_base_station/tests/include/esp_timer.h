#pragma once

/**
 * @file esp_timer.h
 * @brief Host-side stub of esp_timer_get_time() for native tests.
 *
 * tflite_infer.cpp calls esp_timer_get_time() to measure inference
 * latency.  Map it to a simple monotonic clock backed by
 * std::chrono::steady_clock (microseconds).
 */

#include <chrono>
#include <cstdint>

inline int64_t esp_timer_get_time(void) {
    auto now = std::chrono::steady_clock::now();
    auto us = std::chrono::duration_cast<std::chrono::microseconds>(
        now.time_since_epoch()).count();
    return static_cast<int64_t>(us);
}
