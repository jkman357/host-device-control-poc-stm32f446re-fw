// Copyright (c) 2026 Ray Yang. All rights reserved.
//
// File:
//     platform.h
//
// Purpose:
//     Defines the public contract for board and processor services.
//
// Public Contract:
//     - Initializes the reset-safe system clock and NUCLEO-F446RE GPIO.
//     - Starts the fixed five-millisecond TIM6 event source.
//     - Controls the board LED and processor interrupt mask.
//     - Exposes the TIM6 interrupt integration entry point.

#ifndef PLATFORM_H
#define PLATFORM_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PLATFORM_CORE_CLOCK_HZ       (16000000u)
#define PLATFORM_TICK_PERIOD_MS      (5u)

/*
 * Function:
 *     platform_init
 *
 * Purpose:
 *     Initializes the reset-safe HSI clock, GPIO, and board LED.
 *
 * Input Parameters:
 *     None.
 *
 * Output Parameters:
 *     None.
 *
 * Return Value:
 *     true:
 *         Required clock and GPIO initialization completed.
 *     false:
 *         The HSI clock did not become ready within the bounded poll limit.
 */
bool platform_init(void);

/*
 * Function:
 *     platform_start_five_millisecond_timer
 *
 * Purpose:
 *     Configures and starts TIM6 as a five-millisecond application event source.
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
void platform_start_five_millisecond_timer(void);

/*
 * Function:
 *     platform_led_set
 *
 * Purpose:
 *     Sets the NUCLEO LD2 LED state.
 *
 * Input Parameters:
 *     is_on:
 *         True turns the LED on; false turns it off.
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
 *     platform_wait_for_interrupt
 *
 * Purpose:
 *     Waits for an interrupt using the caller-controlled interrupt-mask protocol.
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
 *     Call only after atomically confirming that no event is pending.
 */
void platform_wait_for_interrupt(void);

/*
 * Function:
 *     platform_irq_save
 *
 * Purpose:
 *     Disables maskable interrupts and returns the previous PRIMASK state.
 *
 * Input Parameters:
 *     None.
 *
 * Output Parameters:
 *     None.
 *
 * Return Value:
 *     primask:
 *         Previous PRIMASK value to pass to platform_irq_restore.
 */
uint32_t platform_irq_save(void);

/*
 * Function:
 *     platform_irq_restore
 *
 * Purpose:
 *     Restores a PRIMASK state previously returned by platform_irq_save.
 *
 * Input Parameters:
 *     primask:
 *         Supplies the PRIMASK value to restore.
 *
 * Output Parameters:
 *     None.
 *
 * Return Value:
 *     None.
 */
void platform_irq_restore(uint32_t primask);

/*
 * Function:
 *     platform_tim6_dac_irq_handler
 *
 * Purpose:
 *     Handles the TIM6 update interrupt and posts one application tick.
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
 *     Execution Context: ISR. Blocking: prohibited. Work is bounded.
 */
void platform_tim6_dac_irq_handler(void);

#ifdef __cplusplus
}
#endif

#endif // PLATFORM_H
