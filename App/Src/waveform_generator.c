// Copyright (c) 2026 Ray Yang. All rights reserved.

#include "waveform_generator.h"

#define WAVEFORM_TWO_PI (6.28318531f)
#define WAVEFORM_PI (3.14159265f)
#define WAVEFORM_QUARTER_CYCLE (0.25f)
#define WAVEFORM_HALF_CYCLE (0.50f)
#define WAVEFORM_THREE_QUARTER_CYCLE (0.75f)
#define WAVEFORM_SINE_OUTPUT_SCALE (1.00015692f)

#define ECG_P_CENTER (0.18f)
#define ECG_P_HALF_WIDTH (0.045f)
#define ECG_P_AMPLITUDE (0.12f)
#define ECG_Q_CENTER (0.36f)
#define ECG_Q_HALF_WIDTH (0.018f)
#define ECG_Q_AMPLITUDE (-0.15f)
#define ECG_R_CENTER (0.40f)
#define ECG_R_HALF_WIDTH (0.015f)
#define ECG_R_AMPLITUDE (1.00f)
#define ECG_S_CENTER (0.44f)
#define ECG_S_HALF_WIDTH (0.022f)
#define ECG_S_AMPLITUDE (-0.25f)
#define ECG_T_CENTER (0.68f)
#define ECG_T_HALF_WIDTH (0.080f)
#define ECG_T_AMPLITUDE (0.30f)

static float waveform_normalized_phase(uint32_t phase_us,
                                       uint32_t period_us)
{
    return (float)(phase_us % period_us) / (float)period_us;
}

static float waveform_sine_polynomial(float angle)
{
    const float angle_squared = angle * angle;
    const float polynomial =
        1.0f
        + (angle_squared
           * ((-1.0f / 6.0f)
              + (angle_squared
                 * ((1.0f / 120.0f)
                    + (angle_squared * (-1.0f / 5040.0f))))));

    return angle * polynomial * WAVEFORM_SINE_OUTPUT_SCALE;
}

static float waveform_sine_sample(uint32_t phase_us)
{
    const float phase =
        waveform_normalized_phase(phase_us, WAVEFORM_STANDARD_PERIOD_US);
    float angle;

    if (phase < WAVEFORM_QUARTER_CYCLE)
    {
        angle = phase * WAVEFORM_TWO_PI;
    }
    else if (phase < WAVEFORM_THREE_QUARTER_CYCLE)
    {
        angle = WAVEFORM_PI - (phase * WAVEFORM_TWO_PI);
    }
    else
    {
        angle = (phase * WAVEFORM_TWO_PI) - WAVEFORM_TWO_PI;
    }

    return waveform_sine_polynomial(angle);
}

static float waveform_square_sample(uint32_t phase_us)
{
    const float phase =
        waveform_normalized_phase(phase_us, WAVEFORM_STANDARD_PERIOD_US);

    return (phase < WAVEFORM_HALF_CYCLE) ? 1.0f : -1.0f;
}

static float waveform_triangle_sample(uint32_t phase_us)
{
    const float phase =
        waveform_normalized_phase(phase_us, WAVEFORM_STANDARD_PERIOD_US);

    if (phase < WAVEFORM_QUARTER_CYCLE)
    {
        return 4.0f * phase;
    }
    if (phase < WAVEFORM_THREE_QUARTER_CYCLE)
    {
        return 2.0f - (4.0f * phase);
    }

    return (4.0f * phase) - 4.0f;
}

static float waveform_smooth_pulse(float phase,
                                   float center,
                                   float half_width,
                                   float amplitude)
{
    const float normalized_distance = (phase - center) / half_width;
    float shape;

    if ((normalized_distance <= -1.0f) || (normalized_distance >= 1.0f))
    {
        return 0.0f;
    }

    shape = 1.0f - (normalized_distance * normalized_distance);
    return amplitude * shape * shape;
}

static float waveform_ecg_sample(uint32_t phase_us)
{
    const float phase = waveform_normalized_phase(
        phase_us,
        WAVEFORM_ECG_70_BPM_PERIOD_US);
    float sample = 0.0f;

    sample += waveform_smooth_pulse(phase,
                                    ECG_P_CENTER,
                                    ECG_P_HALF_WIDTH,
                                    ECG_P_AMPLITUDE);
    sample += waveform_smooth_pulse(phase,
                                    ECG_Q_CENTER,
                                    ECG_Q_HALF_WIDTH,
                                    ECG_Q_AMPLITUDE);
    sample += waveform_smooth_pulse(phase,
                                    ECG_R_CENTER,
                                    ECG_R_HALF_WIDTH,
                                    ECG_R_AMPLITUDE);
    sample += waveform_smooth_pulse(phase,
                                    ECG_S_CENTER,
                                    ECG_S_HALF_WIDTH,
                                    ECG_S_AMPLITUDE);
    sample += waveform_smooth_pulse(phase,
                                    ECG_T_CENTER,
                                    ECG_T_HALF_WIDTH,
                                    ECG_T_AMPLITUDE);

    return sample;
}

float waveform_generator_sample(waveform_type_t waveform,
                                uint32_t phase_us)
{
    switch (waveform)
    {
        case WAVEFORM_TYPE_SINE:
            return waveform_sine_sample(phase_us);

        case WAVEFORM_TYPE_SQUARE:
            return waveform_square_sample(phase_us);

        case WAVEFORM_TYPE_TRIANGLE:
            return waveform_triangle_sample(phase_us);

        case WAVEFORM_TYPE_ECG_70_BPM:
            return waveform_ecg_sample(phase_us);

        default:
            return 0.0f;
    }
}

uint32_t waveform_generator_period_us(waveform_type_t waveform)
{
    if (waveform == WAVEFORM_TYPE_ECG_70_BPM)
    {
        return WAVEFORM_ECG_70_BPM_PERIOD_US;
    }

    return WAVEFORM_STANDARD_PERIOD_US;
}

waveform_type_t waveform_generator_next(waveform_type_t waveform)
{
    switch (waveform)
    {
        case WAVEFORM_TYPE_SINE:
            return WAVEFORM_TYPE_SQUARE;

        case WAVEFORM_TYPE_SQUARE:
            return WAVEFORM_TYPE_TRIANGLE;

        case WAVEFORM_TYPE_TRIANGLE:
            return WAVEFORM_TYPE_ECG_70_BPM;

        case WAVEFORM_TYPE_ECG_70_BPM:
        default:
            return WAVEFORM_TYPE_SINE;
    }
}
