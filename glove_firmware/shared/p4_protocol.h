/* =============================================================================
 * EchoGlove V5.2 — P4 Base Station Protocol Constants
 * =============================================================================
 * Shared constants and types for the P4 (ESP32-P4) base station.
 * Included by both the C6 firmware and P4 base station code.
 * =============================================================================
 */

#ifndef P4_PROTOCOL_H
#define P4_PROTOCOL_H

#include "data_structures.h"

// ── P4 Base Station Constants ────────────────────────────────────
static constexpr uint32_t P4_UART_BAUD        = 2000000;  // 2 Mbps
static constexpr uint32_t P4_INFERENCE_HZ     = 30;
static constexpr uint32_t P4_DISPLAY_HZ       = 10;
static constexpr uint32_t P4_BLE_TIMESLICE_PCT = 10;       // 10% for BLE
static constexpr uint32_t P4_TTS_DEDUP_MS     = 2000;      // same gesture dedup window

// ── Base Station Status ──────────────────────────────────────────
struct BaseStationStatus {
    bool     c6_connected;
    bool     p4_ready;
    float    cpu_usage;
    float    mem_usage;
    uint32_t uptime_s;
    uint8_t  active_tier;  // 1=Tier1 only, 2=Tier2 P4, 3=Tier3 PC
};

#endif // P4_PROTOCOL_H
