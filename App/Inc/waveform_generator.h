// Copyright (c) 2026 Ray Yang. All rights reserved.
//
// File:
//     waveform_generator.h
//
// Purpose:
//     Defines the deterministic waveform-generator interface.
//
// Public Contract:
//     - Defines supported waveform types and periods.
//     - Generates bounded waveform samples from an explicit phase.
//     - Provides deterministic waveform rotation without hidden state.
//
// Notes:
//     The implementation does not allocate memory or access hardware.

#ifndef WAVEFORM_GENERATOR_H
#define WAVEFORM_GENERATOR_H

#include <stdint.h>

typedef enum
{
    WAVEFORM_TYPE_SINE = 0,
    WAVEFORM_TYPE_SQUARE,
    WAVEFORM_TYPE_TRIANGLE,
    WAVEFORM_TYPE_ECG_70_BPM,
    WAVEFORM_TYPE_COUNT
} waveform_type_t;

#define WAVEFORM_STANDARD_PERIOD_US (1000000u)
#define WAVEFORM_ECG_70_BPM_PERIOD_US (857143u)

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
                                uint32_t phase_us);
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
uint32_t waveform_generator_period_us(waveform_type_t waveform);
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
waveform_type_t waveform_generator_next(waveform_type_t waveform);

#endif
