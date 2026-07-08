/* =============================================================================
 * EchoGlove V5.2 — P4 Internal Mock Data Source
 * =============================================================================
 * Generates synthetic L/R GlovePackets and feeds them to FramePairer directly,
 * bypassing the UART/C6 path. Used for standalone P4 hardware verification
 * (LVGL display + USB CDC JSON + TTS audio) without a C6 co-processor or real
 * gloves.
 *
 * Compile-time controlled by CONFIG_P4_INTERNAL_MOCK=y. Mirrors the
 * c6_firmware CONFIG_MOCK_ESP_NOW mock (espnow_handler.cpp) so both sides
 * share the same 5-gesture cycle and L/R alternation pattern.
 *
 * The poll() interface matches uart_receiver_poll() so main.cpp can stay
 * source-agnostic.
 * =============================================================================
 */

#ifndef MOCK_DATA_SOURCE_H
#define MOCK_DATA_SOURCE_H

#include <stdbool.h>
#include <stdint.h>
#include "data_structures.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialize the internal mock data source. Spawns a task that generates
 * synthetic L/R GlovePackets at 50Hz per hand into a queue.
 */
bool mock_data_source_init(void);

/**
 * Poll for the next synthetic packet (non-blocking).
 * @return true if a packet was dequeued into *out.
 */
bool mock_data_source_poll(GlovePacket* out);

/** Number of packets dequeued via poll(). */
uint32_t mock_data_source_count(void);

#ifdef __cplusplus
}
#endif

#endif // MOCK_DATA_SOURCE_H
