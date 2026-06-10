/**
 * @file display_task.cpp
 * @brief LVGL display UI for the P4 base station.
 *
 * Current implementation logs gesture results and system status via
 * ESP_LOGI.  Full LVGL widget creation will be enabled once the BSP
 * (esp-bsp for the P4 EV Board) is integrated.
 *
 * Layout (planned):
 *   ┌─────────────────────────────────────┐
 *   │  LEFT GESTURE        RIGHT GESTURE  │
 *   │  [████████░░] 85%   [██████░░░░] 60%│
 *   │                                     │
 *   │  Flex L: ║║║║║   Flex R: ║║║║║      │
 *   │  Thumb Index Mid Ring Pinky ...     │
 *   │                                     │
 *   │  Status: C6=OK P4=OK Tier=2         │
 *   │  Log: Gesture "你好" 85%            │
 *   └─────────────────────────────────────┘
 */

#include "display_task.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include <cstring>

/* ── LVGL include (guarded until BSP is linked) ────────────────── */
#if __has_include("lvgl.h")
#include "lvgl.h"
#define HAS_LVGL 1
#else
#define HAS_LVGL 0
#endif

static const char* TAG = "display";

/* ── Gesture name table (46 CSL signs) ─────────────────────────── */
static const char* GESTURE_NAMES[46] = {
    /*  0 */ "你好",     /*  1 */ "谢谢",    /*  2 */ "对不起",
    /*  3 */ "是",       /*  4 */ "不是",    /*  5 */ "请",
    /*  6 */ "再见",     /*  7 */ "早上好",  /*  8 */ "晚上好",
    /*  9 */ "快乐",     /* 10 */ "悲伤",    /* 11 */ "生气",
    /* 12 */ "害怕",     /* 13 */ "惊讶",    /* 14 */ "吃饭",
    /* 15 */ "睡觉",     /* 16 */ "工作",    /* 17 */ "学习",
    /* 18 */ "家",       /* 19 */ "学校",    /* 20 */ "朋友",
    /* 21 */ "家人",     /* 22 */ "医生",    /* 23 */ "帮助",
    /* 24 */ "喜欢",     /* 25 */ "不喜欢",  /* 26 */ "大",
    /* 27 */ "小",       /* 28 */ "多",      /* 29 */ "少",
    /* 30 */ "好",       /* 31 */ "坏",      /* 32 */ "新",
    /* 33 */ "旧",       /* 34 */ "快",      /* 35 */ "慢",
    /* 36 */ "左",       /* 37 */ "右",      /* 38 */ "上",
    /* 39 */ "下",       /* 40 */ "来",      /* 41 */ "去",
    /* 42 */ "看",       /* 43 */ "听",      /* 44 */ "说",
    /* 45 */ "想"
};

static const char* FINGER_NAMES[5] = {
    "Thumb", "Index", "Middle", "Ring", "Pinky"
};

/* ── LVGL widget handles (populated in display_init) ───────────── */
#if HAS_LVGL
static lv_obj_t* s_lbl_left_gesture  = NULL;
static lv_obj_t* s_lbl_right_gesture = NULL;
static lv_obj_t* s_bar_left_conf     = NULL;
static lv_obj_t* s_bar_right_conf    = NULL;
static lv_obj_t* s_lbl_status        = NULL;
static lv_obj_t* s_lbl_log           = NULL;
static lv_obj_t* s_flex_bars_left[5]  = {};
static lv_obj_t* s_flex_bars_right[5] = {};
#endif

/* ── Helper: map confidence to 0-100 bar value ─────────────────── */
static inline int32_t conf_to_bar(float conf) {
    int32_t v = (int32_t)(conf * 100.0f);
    return (v < 0) ? 0 : (v > 100) ? 100 : v;
}

/* ================================================================= */
/*  Public API                                                        */
/* ================================================================= */

bool display_init(void) {
    ESP_LOGI(TAG, "Display init (LVGL)");

#if HAS_LVGL
    /* -------------------------------------------------------------- */
    /*  Full LVGL BSP init should happen here via esp-bsp:
     *      bsp_display_start();
     *      bsp_display_lock();
     *      // create widgets ...
     *      bsp_display_unlock();
     *
     *  The widget skeleton below shows the intended layout.            */
    /* -------------------------------------------------------------- */

    lv_obj_t* scr = lv_scr_act();

    /* -- Left gesture label -- */
    s_lbl_left_gesture = lv_label_create(scr);
    lv_label_set_text(s_lbl_left_gesture, "--");
    lv_obj_set_pos(s_lbl_left_gesture, 10, 10);

    /* -- Right gesture label -- */
    s_lbl_right_gesture = lv_label_create(scr);
    lv_label_set_text(s_lbl_right_gesture, "--");
    lv_obj_set_pos(s_lbl_right_gesture, 200, 10);

    /* -- Left confidence bar -- */
    s_bar_left_conf = lv_bar_create(scr);
    lv_bar_set_range(s_bar_left_conf, 0, 100);
    lv_bar_set_value(s_bar_left_conf, 0, LV_ANIM_OFF);
    lv_obj_set_size(s_bar_left_conf, 150, 15);
    lv_obj_set_pos(s_bar_left_conf, 10, 35);

    /* -- Right confidence bar -- */
    s_bar_right_conf = lv_bar_create(scr);
    lv_bar_set_range(s_bar_right_conf, 0, 100);
    lv_bar_set_value(s_bar_right_conf, 0, LV_ANIM_OFF);
    lv_obj_set_size(s_bar_right_conf, 150, 15);
    lv_obj_set_pos(s_bar_right_conf, 200, 35);

    /* -- Flex bars: left hand (5 fingers) -- */
    for (int i = 0; i < 5; i++) {
        s_flex_bars_left[i] = lv_bar_create(scr);
        lv_bar_set_range(s_flex_bars_left[i], 0, 100);
        lv_bar_set_value(s_flex_bars_left[i], 0, LV_ANIM_OFF);
        lv_obj_set_size(s_flex_bars_left[i], 80, 10);
        lv_obj_set_pos(s_flex_bars_left[i], 10, 70 + i * 15);
    }

    /* -- Flex bars: right hand (5 fingers) -- */
    for (int i = 0; i < 5; i++) {
        s_flex_bars_right[i] = lv_bar_create(scr);
        lv_bar_set_range(s_flex_bars_right[i], 0, 100);
        lv_bar_set_value(s_flex_bars_right[i], 0, LV_ANIM_OFF);
        lv_obj_set_size(s_flex_bars_right[i], 80, 10);
        lv_obj_set_pos(s_flex_bars_right[i], 200, 70 + i * 15);
    }

    /* -- Status label -- */
    s_lbl_status = lv_label_create(scr);
    lv_label_set_text(s_lbl_status, "Status: --");
    lv_obj_set_pos(s_lbl_status, 10, 160);

    /* -- Log label -- */
    s_lbl_log = lv_label_create(scr);
    lv_label_set_text(s_lbl_log, "Log: idle");
    lv_obj_set_pos(s_lbl_log, 10, 185);

    ESP_LOGI(TAG, "LVGL widgets created");
#else
    ESP_LOGW(TAG, "lvgl.h not found -- display widgets skipped (log-only mode)");
#endif

    return true;
}

void display_update(const Tier2Result* result,
                    const FramePair* pair,
                    const float features[DUAL_HAND_FEATURES],
                    const BaseStationStatus* status) {
    /* ---- Always log regardless of LVGL availability ---- */

    if (result && result->valid) {
        const char* name = (result->gesture_id >= 0 && result->gesture_id < 46)
                           ? GESTURE_NAMES[result->gesture_id]
                           : "Unknown";
        ESP_LOGI(TAG, "Gesture: %s (%.0f%%) [%lu us]",
                 name, result->confidence * 100.0f,
                 (unsigned long)result->inference_us);
    }

    if (features) {
        ESP_LOGD(TAG, "Flex L: %.2f %.2f %.2f %.2f %.2f",
                 features[0], features[1], features[2], features[3], features[4]);
        ESP_LOGD(TAG, "Flex R: %.2f %.2f %.2f %.2f %.2f",
                 features[11], features[12], features[13], features[14], features[15]);
    }

    if (status) {
        ESP_LOGI(TAG, "Status: C6=%d P4=%d Tier=%d cpu=%.0f%% mem=%.0f%%",
                 status->c6_connected, status->p4_ready,
                 status->active_tier, status->cpu_usage, status->mem_usage);
    }

    /* ---- Update LVGL widgets if available ---- */
#if HAS_LVGL
    if (result && result->valid) {
        const char* name = (result->gesture_id >= 0 && result->gesture_id < 46)
                           ? GESTURE_NAMES[result->gesture_id]
                           : "?";
        /* Assume single-gesture result displayed on left for now;
         * dual-hand split will be added when Tier2 outputs per-hand. */
        if (s_lbl_left_gesture)
            lv_label_set_text(s_lbl_left_gesture, name);
        if (s_bar_left_conf)
            lv_bar_set_value(s_bar_left_conf, conf_to_bar(result->confidence), LV_ANIM_ON);
    }

    if (features) {
        for (int i = 0; i < 5; i++) {
            if (s_flex_bars_left[i])
                lv_bar_set_value(s_flex_bars_left[i],
                                 (int32_t)(features[i] * 100.0f), LV_ANIM_OFF);
            if (s_flex_bars_right[i])
                lv_bar_set_value(s_flex_bars_right[i],
                                 (int32_t)(features[11 + i] * 100.0f), LV_ANIM_OFF);
        }
    }

    if (status && s_lbl_status) {
        char buf[64];
        snprintf(buf, sizeof(buf), "C6=%s P4=%s T%d",
                 status->c6_connected ? "OK" : "--",
                 status->p4_ready     ? "OK" : "--",
                 status->active_tier);
        lv_label_set_text(s_lbl_status, buf);
    }

    if (result && result->valid && s_lbl_log) {
        const char* name = (result->gesture_id >= 0 && result->gesture_id < 46)
                           ? GESTURE_NAMES[result->gesture_id]
                           : "?";
        char buf[48];
        snprintf(buf, sizeof(buf), "%s %.0f%%", name, result->confidence * 100.0f);
        lv_label_set_text(s_lbl_log, buf);
    }
#endif /* HAS_LVGL */
}
