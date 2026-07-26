// Copyright (c) 2026 Ray Yang. All rights reserved.

#include <assert.h>
#include <stdint.h>

#include "waveform_generator.h"

static float absolute_value(float value)
{
    return (value < 0.0f) ? -value : value;
}

static void assert_near(float actual, float expected, float tolerance)
{
    assert(absolute_value(actual - expected) <= tolerance);
}

int main(void)
{
    const uint32_t ecg_period =
        waveform_generator_period_us(WAVEFORM_TYPE_ECG_70_BPM);

    assert(waveform_generator_period_us(WAVEFORM_TYPE_SINE)
           == WAVEFORM_STANDARD_PERIOD_US);
    assert(ecg_period == WAVEFORM_ECG_70_BPM_PERIOD_US);

    assert_near(waveform_generator_sample(WAVEFORM_TYPE_SINE, 0u),
                0.0f,
                0.0001f);
    assert_near(waveform_generator_sample(WAVEFORM_TYPE_SINE, 250000u),
                1.0f,
                0.0002f);
    assert_near(waveform_generator_sample(WAVEFORM_TYPE_SINE, 500000u),
                0.0f,
                0.0002f);
    assert_near(waveform_generator_sample(WAVEFORM_TYPE_SINE, 750000u),
                -1.0f,
                0.0002f);
    assert((1.0f - waveform_generator_sample(WAVEFORM_TYPE_SINE, 245000u))
           < 0.0010f);
    assert((1.0f - waveform_generator_sample(WAVEFORM_TYPE_SINE, 255000u))
           < 0.0010f);

    assert_near(waveform_generator_sample(WAVEFORM_TYPE_SQUARE, 0u),
                1.0f,
                0.0f);
    assert_near(waveform_generator_sample(WAVEFORM_TYPE_SQUARE, 500000u),
                -1.0f,
                0.0f);

    assert_near(waveform_generator_sample(WAVEFORM_TYPE_TRIANGLE, 0u),
                0.0f,
                0.0f);
    assert_near(waveform_generator_sample(WAVEFORM_TYPE_TRIANGLE, 250000u),
                1.0f,
                0.0f);
    assert_near(waveform_generator_sample(WAVEFORM_TYPE_TRIANGLE, 750000u),
                -1.0f,
                0.0f);

    assert_near(waveform_generator_sample(WAVEFORM_TYPE_ECG_70_BPM,
                                          (ecg_period * 18u) / 100u),
                0.12f,
                0.002f);
    assert_near(waveform_generator_sample(WAVEFORM_TYPE_ECG_70_BPM,
                                          (ecg_period * 36u) / 100u),
                -0.15f,
                0.003f);
    assert_near(waveform_generator_sample(WAVEFORM_TYPE_ECG_70_BPM,
                                          (ecg_period * 40u) / 100u),
                1.0f,
                0.003f);
    assert_near(waveform_generator_sample(WAVEFORM_TYPE_ECG_70_BPM,
                                          (ecg_period * 44u) / 100u),
                -0.25f,
                0.003f);
    assert_near(waveform_generator_sample(WAVEFORM_TYPE_ECG_70_BPM,
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

    return 0;
}
