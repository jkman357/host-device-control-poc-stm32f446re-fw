// Copyright (c) 2026 Ray Yang. All rights reserved.
//
// File:
//     sine_generator.h
//
// Purpose:
//     Defines the public deterministic sine-wave sample contract.
//
// Public Contract:
//     - Returns one IEEE-754 binary32 sine value for a microsecond phase.
//     - Uses a one-second period and one-millisecond lookup resolution.
//     - Retains no mutable generator state.

#ifndef SINE_GENERATOR_H
#define SINE_GENERATOR_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Function:
 *     sine_generator_get_sample
 *
 * Purpose:
 *     Returns the one-hertz sine value for a microsecond phase.
 *
 * Input Parameters:
 *     phase_us:
 *         Supplies the phase in microseconds. Values are reduced modulo one second.
 *
 * Output Parameters:
 *     None.
 *
 * Return Value:
 *     result:
 *         IEEE-754 binary32 sine value in the inclusive range minus one to one.
 */
float sine_generator_get_sample(uint32_t phase_us);

#ifdef __cplusplus
}
#endif

#endif // SINE_GENERATOR_H
