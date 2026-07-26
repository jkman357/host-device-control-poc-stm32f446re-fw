// Copyright (c) 2026 Ray Yang. All rights reserved.
//
// File:
//     waveform_generator.c
//
// Purpose:
//     Implements deterministic synthetic waveform generation.
//
// Responsibilities:
//     - Generates sine, square, triangle, and synthetic 70 bpm ECG samples.
//     - Keeps all calculations bounded and independent of dynamic memory.
//     - Provides deterministic waveform-period and rotation behavior.
//
// Notes:
//     The ECG waveform is synthetic test data and is not a diagnostic signal.

#include "waveform_generator.h"

static const float s_waveform_two_pi = 6.28318531f;
static const float s_waveform_pi = 3.14159265f;
static const float s_waveform_quarter_cycle = 0.25f;
static const float s_waveform_half_cycle = 0.50f;
static const float s_waveform_three_quarter_cycle = 0.75f;
static const float s_waveform_sine_output_scale = 1.00015692f;
static const float s_waveform_sine_coefficient_3 = -0.16666667f;
static const float s_waveform_sine_coefficient_5 = 0.00833333f;
static const float s_waveform_sine_coefficient_7 = -0.00019841f;
static const float s_waveform_triangle_rising_slope = 4.0f;
static const float s_waveform_triangle_falling_intercept = 2.0f;
static const float s_waveform_triangle_final_intercept = 4.0f;

static const float s_ecg_p_center = 0.18f;
static const float s_ecg_p_half_width = 0.045f;
static const float s_ecg_p_amplitude = 0.12f;
static const float s_ecg_q_center = 0.36f;
static const float s_ecg_q_half_width = 0.018f;
static const float s_ecg_q_amplitude = -0.15f;
static const float s_ecg_r_center = 0.40f;
static const float s_ecg_r_half_width = 0.015f;
static const float s_ecg_r_amplitude = 1.00f;
static const float s_ecg_s_center = 0.44f;
static const float s_ecg_s_half_width = 0.022f;
static const float s_ecg_s_amplitude = -0.25f;
static const float s_ecg_t_center = 0.68f;
static const float s_ecg_t_half_width = 0.080f;
static const float s_ecg_t_amplitude = 0.30f;

/*
 * Function:
 *     waveform_generator_normalized_phase
 *
 * Purpose:
 *     Converts an elapsed microsecond phase to the normalized interval [0, 1).
 *
 * Input Parameters:
 *     phase_us:
 *         Elapsed phase in microseconds.
 *     period_us:
 *         Nonzero waveform period in microseconds.
 *
 * Output Parameters:
 *     None.
 *
 * Return Value:
 *     Normalized phase in the interval [0, 1).
 */
static float waveform_generator_normalized_phase(uint32_t phase_us,
                                       uint32_t period_us)
{
    if (period_us == 0u)
    {
        return 0.0f;
    }

    return (float)(phase_us % period_us) / (float)period_us;
}

/*
 * Function:
 *     waveform_generator_sine_polynomial
 *
 * Purpose:
 *     Approximates sine over the range-reduced angle used by the generator.
 *
 * Input Parameters:
 *     angle:
 *         Range-reduced angle in radians.
 *
 * Output Parameters:
 *     None.
 *
 * Return Value:
 *     Approximated sine value scaled to the normalized output range.
 */
static float waveform_generator_sine_polynomial(float angle)
{
    const float angle_squared = angle * angle;
    const float polynomial =
        1.0f
        + (angle_squared
           * (s_waveform_sine_coefficient_3
              + (angle_squared
                 * (s_waveform_sine_coefficient_5
                    + (angle_squared * s_waveform_sine_coefficient_7)))));

    return angle * polynomial * s_waveform_sine_output_scale;
}

/*
 * Function:
 *     waveform_generator_sine_sample
 *
 * Purpose:
 *     Generates one smooth one-hertz sine-wave sample.
 *
 * Input Parameters:
 *     phase_us:
 *         Elapsed waveform phase in microseconds.
 *
 * Output Parameters:
 *     None.
 *
 * Return Value:
 *     Normalized sine sample.
 */
static float waveform_generator_sine_sample(uint32_t phase_us)
{
    const float phase =
        waveform_generator_normalized_phase(phase_us, WAVEFORM_STANDARD_PERIOD_US);
    float angle;

    if (phase < s_waveform_quarter_cycle)
    {
        angle = phase * s_waveform_two_pi;
    }
    else if (phase < s_waveform_three_quarter_cycle)
    {
        angle = s_waveform_pi - (phase * s_waveform_two_pi);
    }
    else
    {
        angle = (phase * s_waveform_two_pi) - s_waveform_two_pi;
    }

    return waveform_generator_sine_polynomial(angle);
}

/*
 * Function:
 *     waveform_generator_square_sample
 *
 * Purpose:
 *     Generates one bipolar one-hertz square-wave sample.
 *
 * Input Parameters:
 *     phase_us:
 *         Elapsed waveform phase in microseconds.
 *
 * Output Parameters:
 *     None.
 *
 * Return Value:
 *     Positive or negative full-scale sample.
 */
static float waveform_generator_square_sample(uint32_t phase_us)
{
    const float phase =
        waveform_generator_normalized_phase(phase_us, WAVEFORM_STANDARD_PERIOD_US);

    return (phase < s_waveform_half_cycle) ? 1.0f : -1.0f;
}

/*
 * Function:
 *     waveform_generator_triangle_sample
 *
 * Purpose:
 *     Generates one bipolar one-hertz triangle-wave sample.
 *
 * Input Parameters:
 *     phase_us:
 *         Elapsed waveform phase in microseconds.
 *
 * Output Parameters:
 *     None.
 *
 * Return Value:
 *     Normalized triangle-wave sample.
 */
static float waveform_generator_triangle_sample(uint32_t phase_us)
{
    const float phase =
        waveform_generator_normalized_phase(phase_us, WAVEFORM_STANDARD_PERIOD_US);

    if (phase < s_waveform_quarter_cycle)
    {
        return s_waveform_triangle_rising_slope * phase;
    }
    if (phase < s_waveform_three_quarter_cycle)
    {
        return s_waveform_triangle_falling_intercept
             - (s_waveform_triangle_rising_slope * phase);
    }

    return (s_waveform_triangle_rising_slope * phase)
         - s_waveform_triangle_final_intercept;
}

/*
 * Function:
 *     waveform_generator_smooth_pulse
 *
 * Purpose:
 *     Generates one compact smooth pulse used to synthesize an ECG component.
 *
 * Input Parameters:
 *     phase:
 *         Normalized ECG phase.
 *     center:
 *         Normalized center of the pulse.
 *     half_width:
 *         Positive normalized half-width of the pulse.
 *     amplitude:
 *         Signed pulse amplitude.
 *
 * Output Parameters:
 *     None.
 *
 * Return Value:
 *     Pulse sample at the supplied phase, or zero outside the pulse window.
 */
static float waveform_generator_smooth_pulse(float phase,
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

/*
 * Function:
 *     waveform_generator_ecg_sample
 *
 * Purpose:
 *     Generates one synthetic 70 bpm ECG sample from bounded smooth pulses.
 *
 * Input Parameters:
 *     phase_us:
 *         Elapsed ECG phase in microseconds.
 *
 * Output Parameters:
 *     None.
 *
 * Return Value:
 *     Synthetic ECG sample for test visualization.
 */
static float waveform_generator_ecg_sample(uint32_t phase_us)
{
    const float phase = waveform_generator_normalized_phase(
        phase_us,
        WAVEFORM_ECG_70_BPM_PERIOD_US);
    float sample = 0.0f;

    sample += waveform_generator_smooth_pulse(phase,
                                    s_ecg_p_center,
                                    s_ecg_p_half_width,
                                    s_ecg_p_amplitude);
    sample += waveform_generator_smooth_pulse(phase,
                                    s_ecg_q_center,
                                    s_ecg_q_half_width,
                                    s_ecg_q_amplitude);
    sample += waveform_generator_smooth_pulse(phase,
                                    s_ecg_r_center,
                                    s_ecg_r_half_width,
                                    s_ecg_r_amplitude);
    sample += waveform_generator_smooth_pulse(phase,
                                    s_ecg_s_center,
                                    s_ecg_s_half_width,
                                    s_ecg_s_amplitude);
    sample += waveform_generator_smooth_pulse(phase,
                                    s_ecg_t_center,
                                    s_ecg_t_half_width,
                                    s_ecg_t_amplitude);

    return sample;
}

/*
 * Function:
 *     waveform_generator_sample
 *
 * Purpose:
 *     Generates one bounded sample for the selected waveform and phase.
 *
 * Input Parameters:
 *     waveform:
 *         Waveform type to generate.
 *     phase_us:
 *         Elapsed phase in microseconds; values beyond one period wrap.
 *
 * Output Parameters:
 *     None.
 *
 * Return Value:
 *     The generated normalized floating-point sample.
 *     An unsupported waveform value returns zero.
 */
float waveform_generator_sample(waveform_type_t waveform,
                                uint32_t phase_us)
{
    switch (waveform)
    {
        case WAVEFORM_TYPE_SINE:
            return waveform_generator_sine_sample(phase_us);

        case WAVEFORM_TYPE_SQUARE:
            return waveform_generator_square_sample(phase_us);

        case WAVEFORM_TYPE_TRIANGLE:
            return waveform_generator_triangle_sample(phase_us);

        case WAVEFORM_TYPE_ECG_70_BPM:
            return waveform_generator_ecg_sample(phase_us);

        default:
            return 0.0f;
    }
}

/*
 * Function:
 *     waveform_generator_period_us
 *
 * Purpose:
 *     Returns the period associated with the selected waveform.
 *
 * Input Parameters:
 *     waveform:
 *         Waveform type whose period is requested.
 *
 * Output Parameters:
 *     None.
 *
 * Return Value:
 *     The waveform period in microseconds.
 *     Unsupported values use the standard waveform period.
 */
uint32_t waveform_generator_period_us(waveform_type_t waveform)
{
    if (waveform == WAVEFORM_TYPE_ECG_70_BPM)
    {
        return WAVEFORM_ECG_70_BPM_PERIOD_US;
    }

    return WAVEFORM_STANDARD_PERIOD_US;
}

/*
 * Function:
 *     waveform_generator_next
 *
 * Purpose:
 *     Returns the next waveform in the deterministic rotation sequence.
 *
 * Input Parameters:
 *     waveform:
 *         Current waveform type.
 *
 * Output Parameters:
 *     None.
 *
 * Return Value:
 *     The next waveform type; unsupported values restart at sine.
 */
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
