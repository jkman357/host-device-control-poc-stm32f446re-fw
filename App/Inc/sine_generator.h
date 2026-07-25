// Copyright (c) 2026 Ray Yang. All rights reserved.
//
// File:
//     sine_generator.h
//
// Purpose:
//     Defines the public contract for deterministic sine-wave sample generation.
//
// Public Contract:
//     - Resets the waveform sequence.
//     - Returns one bounded signed sample per call.
//     - Keeps waveform data and indexing private to sine_generator.c.

#ifndef SINE_GENERATOR_H
#define SINE_GENERATOR_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Function:
 *     sine_generator_reset
 *
 * Purpose:
 *     Reset the sine generator to sample zero.
 *
 * Input Parameters:
 *     None.
 *
 * Output Parameters:
 *     None.
 *
 * Return Value:
 *     None.
 */
void sine_generator_reset(void);

/*
 * Function:
 *     sine_generator_get_next_sample
 *
 * Purpose:
 *     Return the current sample and advance to the next sample.
 *
 * Input Parameters:
 *     None.
 *
 * Output Parameters:
 *     None.
 *
 * Return Value:
 *     result:
 *         Signed 16-bit sine sample with nominal amplitude 10000.
 */
int16_t sine_generator_get_next_sample(void);

#ifdef __cplusplus
}
#endif

#endif // SINE_GENERATOR_H
