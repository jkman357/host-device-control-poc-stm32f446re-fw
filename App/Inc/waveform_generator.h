// Copyright (c) 2026 Ray Yang. All rights reserved.

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

float waveform_generator_sample(waveform_type_t waveform,
                                uint32_t phase_us);
uint32_t waveform_generator_period_us(waveform_type_t waveform);
waveform_type_t waveform_generator_next(waveform_type_t waveform);

#endif
