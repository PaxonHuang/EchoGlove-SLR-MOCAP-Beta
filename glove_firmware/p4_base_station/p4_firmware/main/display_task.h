#pragma once
/**
 * @file display_task.h
 * @brief LVGL display UI for the P4 base station.
 *
 * Shows dual-hand gesture results, confidence bars, flex sensor
 * readouts, and system status on the P4 EV Board LCD.
 *
 * NOTE: Full LVGL port init (display driver, touch, etc.) depends on
 * the BSP for the specific ESP32-P4 EV Board.  The widget layout and
 * update logic below are board-independent and will be wired to the
 * BSP once hardware is available.
 */

#include "tflite_infer.h"
#include "data_structures.h"
#include "p4_protocol.h"
#include "FramePairer.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialise LVGL and create the display widget tree.
 *
 * Called once from app_main() after task creation.  Returns true on
 * success; if the BSP is not linked yet, it logs a warning and returns
 * true (non-fatal — the rest of the system still runs).
 */
bool display_init(void);

/**
 * @brief Refresh all on-screen widgets with the latest data.
 *
 * Intended to be called at P4_DISPLAY_HZ (10 Hz) from the main loop
 * or a dedicated display task.
 *
 * @param result   Latest Tier2 inference result (may be NULL).
 * @param pair     Latest FramePair that produced the result (may be NULL).
 * @param features Full 28-dim feature vector (may be NULL).
 * @param status   System status struct (may be NULL).
 */
void display_update(const Tier2Result* result,
                    const FramePair* pair,
                    const float features[DUAL_HAND_FEATURES],
                    const BaseStationStatus* status);

#ifdef __cplusplus
}
#endif
