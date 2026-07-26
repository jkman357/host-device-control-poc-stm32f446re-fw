// Copyright (c) 2026 Ray Yang. All rights reserved.
//
// File:
//     waveform_generator_test.c
//
// Purpose:
//     Verifies deterministic waveform-generator output.
//
// Responsibilities:
//     - Checks sine smoothness and key phase values.
//     - Checks square, triangle, and synthetic ECG landmarks.
//     - Checks period selection and waveform rotation.
//
// Notes:
//     This host test uses assertions and does not execute on the MCU target.

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>

#include "waveform_generator.h"

/*
 * Function:
 *     waveform_test_absolute_value
 *
 * Purpose:
 *     Returns the magnitude of one floating-point test value.
 *
 * Input Parameters:
 *     value:
 *         Value whose magnitude is requested.
 *
 * Output Parameters:
 *     None.
 *
 * Return Value:
 *     Absolute magnitude of value.
 */
static float waveform_test_absolute_value(float value)
{
    return (value < 0.0f) ? -value : value;
}

/*
 * Function:
 *     waveform_test_assert_near
 *
 * Purpose:
 *     Asserts that two floating-point values differ by no more than a tolerance.
 *
 * Input Parameters:
 *     actual:
 *         Observed value.
 *     expected:
 *         Expected value.
 *     tolerance:
 *         Maximum permitted absolute difference.
 *
 * Output Parameters:
 *     None.
 *
 * Return Value:
 *     None.
 */
static void waveform_test_assert_near(float actual, float expected, float tolerance)
{
    assert(waveform_test_absolute_value(actual - expected) <= tolerance);
}

/*
 * Function:
 *     main
 *
 * Purpose:
 *     Executes the deterministic waveform-generator host test.
 *
 * Input Parameters:
 *     None.
 *
 * Output Parameters:
 *     None.
 *
 * Return Value:
 *     EXIT_SUCCESS when all assertions pass.
 *
 * Notes:
 *     Assertion failure terminates the host process.
 */
int main(void)
{
    const uint32_t ecg_period =
        waveform_generator_period_us(WAVEFORM_TYPE_ECG_70_BPM);

    assert(waveform_generator_period_us(WAVEFORM_TYPE_SINE)
           == WAVEFORM_STANDARD_PERIOD_US);
    assert(ecg_period == WAVEFORM_ECG_70_BPM_PERIOD_US);

    waveform_test_assert_near(waveform_generator_sample(WAVEFORM_TYPE_SINE, 0u),
                0.0f,
                0.0001f);
    waveform_test_assert_near(waveform_generator_sample(WAVEFORM_TYPE_SINE, 250000u),
                1.0f,
                0.0002f);
    waveform_test_assert_near(waveform_generator_sample(WAVEFORM_TYPE_SINE, 500000u),
                0.0f,
                0.0002f);
    waveform_test_assert_near(waveform_generator_sample(WAVEFORM_TYPE_SINE, 750000u),
                -1.0f,
                0.0002f);
    assert((1.0f - waveform_generator_sample(WAVEFORM_TYPE_SINE, 245000u))
           < 0.0010f);
    assert((1.0f - waveform_generator_sample(WAVEFORM_TYPE_SINE, 255000u))
           < 0.0010f);

    waveform_test_assert_near(waveform_generator_sample(WAVEFORM_TYPE_SQUARE, 0u),
                1.0f,
                0.0f);
    waveform_test_assert_near(waveform_generator_sample(WAVEFORM_TYPE_SQUARE, 500000u),
                -1.0f,
                0.0f);

    waveform_test_assert_near(waveform_generator_sample(WAVEFORM_TYPE_TRIANGLE, 0u),
                0.0f,
                0.0f);
    waveform_test_assert_near(waveform_generator_sample(WAVEFORM_TYPE_TRIANGLE, 250000u),
                1.0f,
                0.0f);
    waveform_test_assert_near(waveform_generator_sample(WAVEFORM_TYPE_TRIANGLE, 750000u),
                -1.0f,
                0.0f);

    waveform_test_assert_near(waveform_generator_sample(WAVEFORM_TYPE_ECG_70_BPM,
                                          (ecg_period * 18u) / 100u),
                0.12f,
                0.002f);
    waveform_test_assert_near(waveform_generator_sample(WAVEFORM_TYPE_ECG_70_BPM,
                                          (ecg_period * 36u) / 100u),
                -0.15f,
                0.003f);
    waveform_test_assert_near(waveform_generator_sample(WAVEFORM_TYPE_ECG_70_BPM,
                                          (ecg_period * 40u) / 100u),
                1.0f,
                0.003f);
    waveform_test_assert_near(waveform_generator_sample(WAVEFORM_TYPE_ECG_70_BPM,
                                          (ecg_period * 44u) / 100u),
                -0.25f,
                0.003f);
    waveform_test_assert_near(waveform_generator_sample(WAVEFORM_TYPE_ECG_70_BPM,
                                          (ecg_period * 68u) / 100u),
                0.30f,
                0.003f);

    assert(waveform_generator_next(WAVEFORM_TYPE_SINE)
           == WAVEFORM_TYPE_SQUARE);
    assert(waveform_generator_next(WAVEFORM_TYPE_SQUARE)
           == WAVEFORM_TYPE_TRIANGLE);
    assert(waveform_generator_next(WAVEFORM_TYPE_TRIANGLE)
           == WAVEFORM_TYPE_ECG_70_BPM);
    assert(waveform_generator_next(WAVEFORM_TYPE_ECG_70_BPM)
           == WAVEFORM_TYPE_SINE);

    return EXIT_SUCCESS;
}
