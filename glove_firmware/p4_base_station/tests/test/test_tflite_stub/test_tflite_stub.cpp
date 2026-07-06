#include <unity.h>
#include "tflite_infer.h"

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
    // Stub must cycle through 5 successive gestures, incrementing by 1 each
    // call (mod 5). The absolute start offset is irrelevant — a prior test in
    // the same process may have advanced the internal counter — but the
    // *pattern* (g, g+1, g+2, g+3, g+4, then wrap to g) must hold.
    int g0 = tflite_run(left, right).gesture_id;
    int g1 = tflite_run(left, right).gesture_id;
    int g2 = tflite_run(left, right).gesture_id;
    int g3 = tflite_run(left, right).gesture_id;
    int g4 = tflite_run(left, right).gesture_id;
    int g5 = tflite_run(left, right).gesture_id;  // wraps to g0
    TEST_ASSERT_GREATER_OR_EQUAL_INT(0, g0);
    TEST_ASSERT_LESS_OR_EQUAL_INT(4, g0);
    TEST_ASSERT_EQUAL_INT((g0 + 1) % 5, g1);
    TEST_ASSERT_EQUAL_INT((g0 + 2) % 5, g2);
    TEST_ASSERT_EQUAL_INT((g0 + 3) % 5, g3);
    TEST_ASSERT_EQUAL_INT((g0 + 4) % 5, g4);
    TEST_ASSERT_EQUAL_INT(g0, g5);  // cycle wraps after 5 calls
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_stub_returns_valid_result);
    RUN_TEST(test_stub_cycles_gestures);
    return UNITY_END();
}
