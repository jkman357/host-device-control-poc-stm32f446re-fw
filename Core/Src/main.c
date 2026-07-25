// Copyright (c) 2026 Ray Yang. All rights reserved.
//
// File:
//     main.c
//
// Purpose:
//     Provides the temporary USART2 loopback firmware entry point.
//
// Responsibilities:
//     - Initializes the platform and interrupt-driven serial transport.
//     - Runs the event-driven foreground dispatcher.
//     - Leaves TIM6 and Shared Protocol processing disabled for isolation.
//     - Enters a defined fail-stop state when initialization fails.

#include "main.h"

#include "app.h"
#include "app_event.h"
#include "platform.h"
#include "serial_transport.h"

/*
 * Function:
 *     main_enter_fail_stop
 *
 * Purpose:
 *     Enters the defined initialization-failure state with interrupts disabled.
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
static void main_enter_fail_stop(void)
{
    // The previous mask is irrelevant because fail-stop never restores interrupts.
    (void)platform_irq_save();
    platform_led_set(false);

    for (;;)
    {
        __asm volatile ("nop");
    }
}

/*
 * Function:
 *     main
 *
 * Purpose:
 *     Initializes and runs the temporary USART2 character-loopback firmware.
 *
 * Input Parameters:
 *     None.
 *
 * Output Parameters:
 *     None.
 *
 * Return Value:
 *     None during normal operation because the dispatcher does not return.
 *
 * Notes:
 *     The name main is required by the freestanding C startup contract.
 */
int main(void)
{
    app_event_batch_t event_batch;
    bool is_initialized;

    is_initialized = platform_init();
    if (is_initialized == false)
    {
        main_enter_fail_stop();
    }

    app_event_init();
    serial_transport_init();
    app_init();

    for (;;)
    {
        if (app_event_take(&event_batch) == true)
        {
            app_process_events(&event_batch);
        }
        else
        {
            app_event_wait();
        }
    }
}
