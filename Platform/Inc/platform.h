// Copyright (c) 2026 Ray Yang. All rights reserved.
//
// File:
//     platform.h
//
// Purpose:
//     Defines the STM32F446RE platform-adapter interface.
//
// Public Contract:
//     - Initializes board clocks, GPIO, timer, and interrupt routing required by the PoC.
//     - Controls the board LED and sample-timer interval.
//     - Exposes the target interrupt entry point required by the vector table.
//
// Notes:
//     Application code shall access target hardware only through this interface.

#ifndef PLATFORM_H
#define PLATFORM_H

#include <stdbool.h>
#include <stdint.h>

/*
 * Function:
 *     platform_init
 *
 * Purpose:
 *     Configures target GPIO, sample timer, and interrupt routing required by the PoC.
 *
 * Input Parameters:
 *     None.
 *
 * Output Parameters:
 *     None.
 *
 * Return Value:
 *     None.
 *
 * Notes:
 *     Call once before enabling application processing.
 */
void platform_init(void);
/*
 * Function:
 *     platform_led_set
 *
 * Purpose:
 *     Sets the NUCLEO user LED to the requested logical state.
 *
 * Input Parameters:
 *     is_on:
 *         true turns the LED on; false turns it off.
 *
 * Output Parameters:
 *     None.
 *
 * Return Value:
 *     None.
 */
void platform_led_set(bool is_on);
/*
 * Function:
 *     platform_sample_timer_set_interval_us
 *
 * Purpose:
 *     Programs the TIM6 update interval in microseconds and restarts the timer period.
 *
 * Input Parameters:
 *     interval_us:
 *         Nonzero timer interval in microseconds.
 *
 * Output Parameters:
 *     None.
 *
 * Return Value:
 *     None.
 */
void platform_sample_timer_set_interval_us(uint16_t interval_us);
/*
 * Function:
 *     TIM6_DAC_IRQHandler
 *
 * Purpose:
 *     Acknowledges a TIM6 update and posts one ordered sample-tick event.
 *
 * Input Parameters:
 *     None.
 *
 * Output Parameters:
 *     None.
 *
 * Return Value:
 *     None.
 *
 * Notes:
 *     Executes in interrupt context and performs bounded non-blocking work.
 */
void TIM6_DAC_IRQHandler(void);

#endif
