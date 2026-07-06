#pragma once

/**
 * @file esp_log.h
 * @brief Host-side stub of the ESP-IDF logging macros for native tests.
 *
 * The real tflite_infer.cpp uses ESP_LOGx macros from esp_log.h.  In a
 * native (host) build we don't have the ESP-IDF toolchain, so map all
 * log macros to printf-style no-ops that still evaluate the format
 * arguments (to keep side effects) but produce no output by default.
 */

#include <cstdio>

#define ESP_LOGE(tag, fmt, ...) do { (void)(tag); (void)printf("E %s: " fmt "\n", tag, ##__VA_ARGS__); } while (0)
#define ESP_LOGW(tag, fmt, ...) do { (void)(tag); (void)printf("W %s: " fmt "\n", tag, ##__VA_ARGS__); } while (0)
#define ESP_LOGI(tag, fmt, ...) do { (void)(tag); (void)printf("I %s: " fmt "\n", tag, ##__VA_ARGS__); } while (0)
#define ESP_LOGD(tag, fmt, ...) do { } while (0)
#define ESP_LOGV(tag, fmt, ...) do { } while (0)
