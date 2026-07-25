// Copyright (c) 2026 Ray Yang. All rights reserved.
//
// File:
//     app_event.c
//
// Purpose:
//     Implements bounded event transfer from interrupt context to the main loop.
//
// Responsibilities:
//     - Coalesces UART event flags.
//     - Counts pending sample-timer ticks without dynamic allocation.
//     - Protects event snapshots with a bounded interrupt critical section.

#include "app_event.h"

#include <limits.h>
#include <stddef.h>

#include "platform.h"

static volatile uint32_t s_event_flags;
static volatile uint16_t s_tick_count;
static volatile uint32_t s_tick_overflow_count;

/*
 * Function:
 *     app_event_increment_saturating_u32
 *
 * Purpose:
 *     Saturating increment for a 32-bit diagnostic counter.
 *
 * Input Parameters:
 *     counter:
 *         Counter to increment.
 *
 * Output Parameters:
 *     None.
 *
 * Return Value:
 *     None.
 */
static void app_event_increment_saturating_u32(volatile uint32_t *counter)
{
    if (*counter < UINT32_MAX)
    {
        *counter += 1u;
    }
}

/*
 * Function:
 *     app_event_init
 *
 * Purpose:
 *     Initialize event state.
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
void app_event_init(void)
{
    s_event_flags = 0u;
    s_tick_count = 0u;
    s_tick_overflow_count = 0u;
}

/*
 * Function:
 *     app_event_post_tick_from_isr
 *
 * Purpose:
 *     Posts one pending sample-timer tick from interrupt context.
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
 *     Execution Context: ISR. Blocking: prohibited. Reentrant: protected
 *         by interrupt serialization. Timing Budget: bounded counter
 *         update.
 */
void app_event_post_tick_from_isr(void)
{
    if (s_tick_count < UINT16_MAX)
    {
        s_tick_count += 1u;
    }
    else
    {
        app_event_increment_saturating_u32(&s_tick_overflow_count);
    }
}

/*
 * Function:
 *     app_event_post_flags_from_isr
 *
 * Purpose:
 *     Adds event flags to the interrupt-owned pending event word.
 *
 * Input Parameters:
 *     flags:
 *         Supplies a bitwise OR of defined APP_EVENT_FLAG values.
 *
 * Output Parameters:
 *     None.
 *
 * Return Value:
 *     None.
 *
 * Notes:
 *     Execution Context: ISR. Blocking: prohibited. Reentrant: protected
 *         by interrupt serialization. Timing Budget: one
 *         read-modify-write operation.
 */
void app_event_post_flags_from_isr(uint32_t flags)
{
    s_event_flags |= flags;
}

/*
 * Function:
 *     app_event_take
 *
 * Purpose:
 *     Atomically transfers all currently pending events to the caller.
 *
 * Input Parameters:
 *     None.
 *
 * Output Parameters:
 *     event_batch:
 *         Receives a coherent event snapshot when the pointer is valid.
 *         The object remains unchanged when the pointer is NULL.
 *
 * Return Value:
 *     true:
 *         At least one event flag or pending tick was transferred.
 *     false:
 *         The pointer was NULL or no event was pending.
 *
 * Notes:
 *     Runs in main context and uses a bounded PRIMASK critical section.
 */
bool app_event_take(app_event_batch_t *event_batch)
{
    uint32_t primask;
    bool has_event;

    if (event_batch == NULL)
    {
        return false;
    }

    primask = platform_irq_save();

    event_batch->flags = s_event_flags;
    event_batch->tick_count = s_tick_count;
    event_batch->tick_overflow_count = s_tick_overflow_count;

    s_event_flags = 0u;
    s_tick_count = 0u;

    platform_irq_restore(primask);

    has_event = ((event_batch->flags != 0u) || (event_batch->tick_count != 0u));
    return has_event;
}

/*
 * Function:
 *     app_event_wait
 *
 * Purpose:
 *     Atomically sleep until an interrupt can make an event pending.
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
void app_event_wait(void)
{
    uint32_t primask;

    primask = platform_irq_save();

    if ((s_event_flags == 0u) && (s_tick_count == 0u))
    {
        platform_wait_for_interrupt();
    }

    platform_irq_restore(primask);
}
