// Copyright (c) 2026 Ray Yang. All rights reserved.

#include "sine_generator.h"

#define SINE_PHASE_PERIOD_US (1000000u)
#define SINE_PHASE_PERIOD_FLOAT (1000000.0f)
#define SINE_QUARTER_CYCLE (0.25f)
#define SINE_THREE_QUARTER_CYCLE (0.75f)
#define SINE_FAST_COEFFICIENT_A (1.27323954f)
#define SINE_FAST_COEFFICIENT_B (0.405284735f)
#define SINE_CORRECTION_COEFFICIENT (0.225f)

float sine_generator_sample(uint32_t phase_us)
{
    float normalized_phase;
    float approximation;
    float magnitude;

    normalized_phase =
        (float)(phase_us % SINE_PHASE_PERIOD_US) / SINE_PHASE_PERIOD_FLOAT;

    if (normalized_phase < SINE_QUARTER_CYCLE)
    {
        approximation = 4.0f * normalized_phase;
    }
    else if (normalized_phase < SINE_THREE_QUARTER_CYCLE)
    {
        approximation = 2.0f - (4.0f * normalized_phase);
    }
    else
    {
        approximation = (4.0f * normalized_phase) - 4.0f;
    }

    magnitude = (approximation < 0.0f) ? -approximation : approximation;
    approximation *=
        SINE_FAST_COEFFICIENT_A - (SINE_FAST_COEFFICIENT_B * magnitude);

    magnitude = (approximation < 0.0f) ? -approximation : approximation;
    approximation += SINE_CORRECTION_COEFFICIENT *
                     ((approximation * magnitude) - approximation);

    return approximation;
}
