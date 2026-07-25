// Copyright (c) 2026 Ray Yang. All rights reserved.
//
// File:
//     main.c
//
// Purpose:
//     Provides the firmware entry point.
//
// Responsibilities:
//     - Initializes platform and firmware modules.
//     - Starts the five-millisecond application timer.
//     - Runs the event-driven superloop.
//     - Enters a defined fail-stop state when required initialization fails.
//
// Notes:
//     Product business logic is not implemented in this file.

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
 *
 * Notes:
 *     The intentional infinite loop owns the processor after initialization failure.
 *     Each iteration performs one bounded no-operation instruction.
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
 *     Initializes the platform and runs the event-driven superloop.
 *
 * Input Parameters:
 *     None.
 *
 * Output Parameters:
 *     None.
 *
 * Return Value:
 *     None during normal operation because the dispatcher loop does not return.
 *
 * Notes:
 *     The name main is required by the freestanding C startup contract.
 */
int main(void)
{
    app_event_batch_t event_batch;
    bool is_platform_ready;

    is_platform_ready = platform_init();
    if (is_platform_ready == false)
    {
        main_enter_fail_stop();
    }

    app_event_init();
    serial_transport_init();
    app_init();
    platform_start_five_millisecond_timer();

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
