/* =============================================================================
 * EchoGlove V5.2 — ESP-NOW Receive Handler (C6 Co-Processor)
 * =============================================================================
 * Receives GlovePackets from ESP32-S3 gloves over ESP-NOW, validates them
 * (magic, version, CRC), and forwards valid packets via callback.
 * =============================================================================
 */

#ifndef ESPNOW_HANDLER_H
#define ESPNOW_HANDLER_H

#include <stdbool.h>
#include <stdint.h>
#include "data_structures.h"

#ifdef __cplusplus
extern "C" {
#endif

// Callback type: called when a valid GlovePacket is received
typedef void (*espnow_packet_cb_t)(const GlovePacket* pkt);

// Initialize ESP-NOW with receive callback
bool espnow_handler_init(espnow_packet_cb_t cb);

// Get number of packets received
uint32_t espnow_handler_rx_count(void);

#ifdef __cplusplus
}
#endif

#endif // ESPNOW_HANDLER_H
