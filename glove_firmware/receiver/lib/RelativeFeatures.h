#pragma once
#include <cmath>
#include <cstring>

class RelativeFeatures {
public:
    void compute(
        const float* left_euler, const float* left_quat,
        const float* right_euler, const float* right_quat,
        float* out,
        const float* left_gyro = nullptr, const float* right_gyro = nullptr
    ) {
        memset(out, 0, 6 * sizeof(float));

        // DeltaEuler[3]
        if (left_euler && right_euler) {
            for (int i = 0; i < 3; i++)
                out[i] = left_euler[i] - right_euler[i];
        }

        // DeltaQuatDist[1]: 2 * arccos(|q_L . q_R|)
        if (left_quat && right_quat) {
            float dot = 0;
            for (int i = 0; i < 4; i++) dot += left_quat[i] * right_quat[i];
            dot = fabsf(dot);
            if (dot > 1.0f) dot = 1.0f;
            out[3] = 2.0f * acosf(dot);
        }

        // DeltaGyroNorm[1] and DeltaGyroAxis[1]
        if (left_gyro && right_gyro) {
            float norm_l = 0, norm_r = 0;
            for (int i = 0; i < 3; i++) {
                norm_l += left_gyro[i] * left_gyro[i];
                norm_r += right_gyro[i] * right_gyro[i];
            }
            out[4] = sqrtf(norm_l) - sqrtf(norm_r);

            float max_diff = 0; int max_idx = 0;
            for (int i = 0; i < 3; i++) {
                float diff = fabsf(left_gyro[i] - right_gyro[i]);
                if (diff > max_diff) { max_diff = diff; max_idx = i; }
            }
            out[5] = max_idx / 3.0f;
        }
    }
};
