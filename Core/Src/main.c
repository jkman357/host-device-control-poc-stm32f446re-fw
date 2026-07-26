// Copyright (c) 2026 Ray Yang. All rights reserved.
//
// File:
//     main.c
//
// Purpose:
//     Provides the firmware entry point.
//
// Responsibilities:
//     - Initializes the event, platform, transport, and application modules.
//     - Runs the bounded application event-dispatch loop.
//
// Notes:
//     Product business logic remains outside this file.

#include "app.h"
#include "app_event.h"
#include "platform.h"
#include "serial_transport.h"

/*
 * Function:
 *     main
 *
 * Purpose:
 *     Initializes firmware modules and runs the application event-dispatch loop.
 *
 * Input Parameters:
 *     None.
 *
 * Output Parameters:
 *     None.
 *
 * Return Value:
 *     The function does not return during normal firmware execution.
 *
 * Notes:
 *     The loop is intentional, bounded per iteration, and executes in main context.
 */
int main(void)
{
    app_event_t event;

    app_event_init();
    platform_init();
    serial_transport_init();
    app_init();

    for (;;)
    {
        if (app_event_take(&event))
        {
            app_process_event(&event);
        }
        else
        {
            app_event_wait();
        }
    }
}
