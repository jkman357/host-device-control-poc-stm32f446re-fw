// Copyright (c) 2026 Ray Yang. All rights reserved.
//
// File:
//     app_event_ordering_test.c
//
// Purpose:
//     Verifies bounded application event-queue ordering and overflow behavior.
//
// Responsibilities:
//     - Checks first-in-first-out delivery across event types.
//     - Checks empty-queue behavior and saturated overflow accounting.
//
// Notes:
//     This host test uses assertions and the host critical-section implementation.

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>

#include "app_event.h"

/*
 * Function:
 *     main
 *
 * Purpose:
 *     Executes the ordered event-queue host test.
 *
 * Input Parameters:
 *     None.
 *
 * Output Parameters:
 *     None.
 *
 * Return Value:
 *     EXIT_SUCCESS when all assertions pass.
 *
 * Notes:
 *     Assertion failure terminates the host process.
 */
int main(void)
{
    app_event_t event;
    uint32_t index;

    app_event_init();

    assert(app_event_post_tick_from_isr());
    assert(app_event_post_rx_byte_from_isr(0x04u));
    assert(app_event_post_tick_from_isr());
    assert(app_event_post_rx_byte_from_isr(0x05u));

    assert(app_event_take(&event));
    assert(event.type == APP_EVENT_TYPE_SAMPLE_TICK);

    assert(app_event_take(&event));
    assert(event.type == APP_EVENT_TYPE_UART_RX_BYTE);
    assert(event.data_byte == 0x04u);

    assert(app_event_take(&event));
    assert(event.type == APP_EVENT_TYPE_SAMPLE_TICK);

    assert(app_event_take(&event));
    assert(event.type == APP_EVENT_TYPE_UART_RX_BYTE);
    assert(event.data_byte == 0x05u);

    assert(!app_event_take(&event));

    app_event_init();
    for (index = 0u; index < (APP_EVENT_QUEUE_CAPACITY - 1u); index += 1u)
    {
        assert(app_event_post_tick_from_isr());
    }

    assert(!app_event_post_tick_from_isr());
    assert(app_event_get_overflow_count() == 1u);

    return EXIT_SUCCESS;
}
