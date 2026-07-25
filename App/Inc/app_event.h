// Copyright (c) 2026 Ray Yang. All rights reserved.
//
// File:
//     app_event.h
//
// Purpose:
//     Defines the public contract for interrupt-to-main event transfer.
//
// Public Contract:
//     - Defines bounded event flags and the event batch structure.
//     - Publishes events from interrupt context.
//     - Transfers pending events atomically to the main context.

#ifndef APP_EVENT_H
#define APP_EVENT_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define APP_EVENT_FLAG_UART_RX_AVAILABLE    (1u << 0u)
#define APP_EVENT_FLAG_UART_ERROR           (1u << 1u)

// Contains one coherent snapshot of events transferred to main context.
typedef struct
{
    uint32_t flags;
    uint16_t tick_count;
    uint32_t tick_overflow_count;
} app_event_batch_t;

/*
 * Function:
 *     app_event_init
 *
 * Purpose:
 *     Initializes pending event state and diagnostic counters.
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
void app_event_init(void);

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
 *     Execution Context: ISR. Blocking: prohibited. Reentrant: protected by
 *     interrupt serialization. Timing Budget: bounded counter update.
 */
void app_event_post_tick_from_isr(void);

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
 *     Execution Context: ISR. Blocking: prohibited. Reentrant: protected by
 *     interrupt serialization. Timing Budget: one read-modify-write operation.
 */
void app_event_post_flags_from_isr(uint32_t flags);

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
 *         Receives a coherent event snapshot when the pointer is valid. The
 *         object remains unchanged when the pointer is NULL.
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
bool app_event_take(app_event_batch_t *event_batch);

/*
 * Function:
 *     app_event_wait
 *
 * Purpose:
 *     Atomically sleeps until an interrupt can make an event pending.
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
 *     Call only from main context after app_event_take returns false.
 */
void app_event_wait(void);

#ifdef __cplusplus
}
#endif

#endif // APP_EVENT_H
