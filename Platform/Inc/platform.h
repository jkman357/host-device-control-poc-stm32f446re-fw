// Copyright (c) 2026 Ray Yang. All rights reserved.

#ifndef PLATFORM_H
#define PLATFORM_H

#include <stdbool.h>
#include <stdint.h>

void platform_init(void);
void platform_led_set(bool is_on);
void platform_sample_timer_set_interval_us(uint16_t interval_us);
void TIM6_DAC_IRQHandler(void);

#endif
