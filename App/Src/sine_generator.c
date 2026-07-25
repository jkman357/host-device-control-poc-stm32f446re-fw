// Copyright (c) 2026 Ray Yang. All rights reserved.
//
// File:
//     sine_generator.c
//
// Purpose:
//     Implements a deterministic one-hertz sine-wave lookup source.
//
// Responsibilities:
//     - Owns the fixed 200-sample waveform table.
//     - Maintains the module-private sample index.
//     - Returns bounded signed 16-bit samples.

#include "sine_generator.h"

#include <stddef.h>

#define SINE_SAMPLE_COUNT       (200u)

static const int16_t s_sine_samples[SINE_SAMPLE_COUNT] =
{
    0, 314, 628, 941, 1253, 1564, 1874, 2181, 2487, 2790,
    3090, 3387, 3681, 3971, 4258, 4540, 4818, 5090, 5358, 5621,
    5878, 6129, 6374, 6613, 6845, 7071, 7290, 7501, 7705, 7902,
    8090, 8271, 8443, 8607, 8763, 8910, 9048, 9178, 9298, 9409,
    9511, 9603, 9686, 9759, 9823, 9877, 9921, 9956, 9980, 9995,
    10000, 9995, 9980, 9956, 9921, 9877, 9823, 9759, 9686, 9603,
    9511, 9409, 9298, 9178, 9048, 8910, 8763, 8607, 8443, 8271,
    8090, 7902, 7705, 7501, 7290, 7071, 6845, 6613, 6374, 6129,
    5878, 5621, 5358, 5090, 4818, 4540, 4258, 3971, 3681, 3387,
    3090, 2790, 2487, 2181, 1874, 1564, 1253, 941, 628, 314,
    0, -314, -628, -941, -1253, -1564, -1874, -2181, -2487, -2790,
    -3090, -3387, -3681, -3971, -4258, -4540, -4818, -5090, -5358, -5621,
    -5878, -6129, -6374, -6613, -6845, -7071, -7290, -7501, -7705, -7902,
    -8090, -8271, -8443, -8607, -8763, -8910, -9048, -9178, -9298, -9409,
    -9511, -9603, -9686, -9759, -9823, -9877, -9921, -9956, -9980, -9995,
    -10000, -9995, -9980, -9956, -9921, -9877, -9823, -9759, -9686, -9603,
    -9511, -9409, -9298, -9178, -9048, -8910, -8763, -8607, -8443, -8271,
    -8090, -7902, -7705, -7501, -7290, -7071, -6845, -6613, -6374, -6129,
    -5878, -5621, -5358, -5090, -4818, -4540, -4258, -3971, -3681, -3387,
    -3090, -2790, -2487, -2181, -1874, -1564, -1253, -941, -628, -314,
};

static uint16_t s_sample_index;

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
void sine_generator_reset(void)
{
    s_sample_index = 0u;
}

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
int16_t sine_generator_get_next_sample(void)
{
    int16_t sample;

    sample = s_sine_samples[s_sample_index];

    if (s_sample_index >= (SINE_SAMPLE_COUNT - 1u))
    {
        s_sample_index = 0u;
    }
    else
    {
        s_sample_index += 1u;
    }

    return sample;
}
