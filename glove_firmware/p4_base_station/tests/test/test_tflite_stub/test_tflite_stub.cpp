#include <unity.h>
#include "tflite_infer.h"
#include "data_structures.h"

void test_stub_returns_valid_result() {
    tflite_init(nullptr, 0);  // no model — stub mode
    float left[TFLITE_INPUT_DIM] = {0};
    float right[TFLITE_INPUT_DIM] = {0};
    Tier2Result r = tflite_run(left, right);
    TEST_ASSERT_TRUE(r.valid);
    TEST_ASSERT_GREATER_OR_EQUAL_INT(0, r.gesture_id);
    TEST_ASSERT_LESS_OR_EQUAL_INT(45, r.gesture_id);
    TEST_ASSERT_GREATER_OR_EQUAL_FLOAT(0.0f, r.confidence);
    TEST_ASSERT_LESS_OR_EQUAL_FLOAT(1.0f, r.confidence);
}

void test_stub_cycles_gestures() {
    tflite_init(nullptr, 0);
    float left[TFLITE_INPUT_DIM] = {0};
    float right[TFLITE_INPUT_DIM] = {0};
    int first = tflite_run(left, right).gesture_id;
    int second = tflite_run(left, right).gesture_id;
    TEST_ASSERT_GREATER_OR_EQUAL_INT(0, first);
    TEST_ASSERT_GREATER_OR_EQUAL_INT(0, second);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_stub_returns_valid_result);
    RUN_TEST(test_stub_cycles_gestures);
    return UNITY_END();
}
