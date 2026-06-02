/* =============================================================================
 * Unit Tests — ADS1115Manager
 * =============================================================================
 * Tests simulation mode: begin, readAll with 5 channels, value range.
 * =============================================================================
 */

#include "unity.h"
#include "ADS1115Manager.h"

ADS1115Manager adc;

void setUp() {}
void tearDown() {}

void test_begin() { TEST_ASSERT_TRUE(adc.begin(true)); }

void test_read_5_channels() {
    adc.begin(true);
    float vals[5] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f};
    adc.setSimulatedValues(vals);
    float out[5];
    TEST_ASSERT_TRUE(adc.readAll(out));
    for (int i = 0; i < 5; i++)
        TEST_ASSERT_FLOAT_WITHIN(0.001f, vals[i], out[i]);
}

void test_values_in_range() {
    adc.begin(true);
    float out[5];
    adc.readAll(out);
    for (int i = 0; i < 5; i++)
        TEST_ASSERT_TRUE(out[i] >= 0.0f && out[i] <= 1.0f);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_begin);
    RUN_TEST(test_read_5_channels);
    RUN_TEST(test_values_in_range);
    return UNITY_END();
}
