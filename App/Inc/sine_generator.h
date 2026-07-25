#ifndef SINE_GENERATOR_H
#define SINE_GENERATOR_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Reset the sine generator to sample zero.
 */
void SineGenerator_Reset(void);

/**
 * @brief Return the current sample and advance to the next sample.
 * @return Signed 16-bit sine sample with nominal amplitude 10000.
 */
int16_t SineGenerator_GetNextSample(void);

#ifdef __cplusplus
}
#endif

#endif /* SINE_GENERATOR_H */
