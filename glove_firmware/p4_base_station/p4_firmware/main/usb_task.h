#pragma once
#include "tflite_infer.h"
#include "data_structures.h"
#include "FramePairer.h"

bool usb_task_init(void);
void usb_task_send_result(const Tier2Result* result,
                           const FramePair* pair,
                           const float features[DUAL_HAND_FEATURES]);
