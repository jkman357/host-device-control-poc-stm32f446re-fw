// Copyright (c) 2026 Ray Yang. All rights reserved.
//
// File:
//     app_event.c
//
// Purpose:
//     Implements the bounded ordered event queue.
//
// Responsibilities:
//     - Serializes UART, timer, and UART-error events posted from interrupt context.
//     - Protects main-context dequeue operations with a bounded critical section.
//     - Counts queue overflow without wrapping.
//
// Notes:
//     Interrupt producers use the same preemption priority on the target.

#include "app_event.h"

#include <limits.h>
#include <stddef.h>

#define APP_EVENT_QUEUE_MASK (APP_EVENT_QUEUE_CAPACITY - 1u)
#define APP_EVENT_DATA_UNUSED (0u)

#if ((APP_EVENT_QUEUE_CAPACITY & APP_EVENT_QUEUE_MASK) != 0u)
#error APP_EVENT_QUEUE_CAPACITY must be a power of two.
#endif

static volatile uint16_t s_head;
static volatile uint16_t s_tail;
static volatile uint32_t s_overflow_count;
static app_event_t s_queue[APP_EVENT_QUEUE_CAPACITY];

#ifndef APP_EVENT_HOST_TEST
/*
 * Function:
 *     app_event_enter_critical
 *
 * Purpose:
 *     Disables interrupts and captures the prior interrupt-mask state.
 *
 * Input Parameters:
 *     None.
 *
 * Output Parameters:
 *     None.
 *
 * Return Value:
 *     The prior PRIMASK value required by app_event_leave_critical.
 *
 * Notes:
 *     Target-only implementation; the host-test branch uses a deterministic placeholder.
 */
static uint32_t app_event_enter_critical(void)
{
    uint32_t primask;

    __asm volatile(
        "MRS %0, primask\n"
        "CPSID i"
        : "=r"(primask)
        :
        : "memory");

    return primask;
}

/*
 * Function:
 *     app_event_leave_critical
 *
 * Purpose:
 *     Restores the interrupt-mask state captured before a bounded critical section.
 *
 * Input Parameters:
 *     primask:
 *         PRIMASK value returned by app_event_enter_critical.
 *
 * Output Parameters:
 *     None.
 *
 * Return Value:
 *     None.
 *
 * Notes:
 *     Target-only implementation; the host-test branch ignores its placeholder value.
 */
static void app_event_leave_critical(uint32_t primask)
{
    __asm volatile("MSR primask, %0" : : "r"(primask) : "memory");
}
#else
/*
 * Function:
 *     app_event_enter_critical
 *
 * Purpose:
 *     Provides the host-test critical-section entry boundary.
 *
 * Input Parameters:
 *     None.
 *
 * Output Parameters:
 *     None.
 *
 * Return Value:
 *     A deterministic placeholder PRIMASK value.
 *
 * Notes:
 *     Host tests execute single-threaded and do not manipulate interrupt state.
 */
static uint32_t app_event_enter_critical(void)
{
    return 0u;
}

/*
 * Function:
 *     app_event_leave_critical
 *
 * Purpose:
 *     Provides the host-test critical-section exit boundary.
 *
 * Input Parameters:
 *     primask:
 *         Placeholder value returned by app_event_enter_critical.
 *
 * Output Parameters:
 *     None.
 *
 * Return Value:
 *     None.
 *
 * Notes:
 *     Host tests execute single-threaded and do not manipulate interrupt state.
 */
static void app_event_leave_critical(uint32_t primask)
{
    (void)primask;
}
#endif

/*
 * Function:
 *     app_event_post_from_isr
 *
 * Purpose:
 *     Posts one bounded event while preserving producer order.
 *
 * Input Parameters:
 *     type:
 *         Event type to enqueue.
 *     data_byte:
 *         Associated data byte, or the defined unused value.
 *
 * Output Parameters:
 *     None.
 *
 * Return Value:
 *     true:
 *         The event was queued.
 *     false:
 *         The queue was full and the overflow counter was updated.
 *
 * Notes:
 *     Called only from interrupt-context wrapper functions.
 */
static bool app_event_post_from_isr(app_event_type_t type, uint8_t data_byte)
{
    uint16_t next_head;

    next_head = (uint16_t)((s_head + 1u) & APP_EVENT_QUEUE_MASK);
    if (next_head == s_tail)
    {
        if (s_overflow_count < UINT32_MAX)
        {
            s_overflow_count += 1u;
        }
        return false;
    }

    s_queue[s_head].type = type;
    s_queue[s_head].data_byte = data_byte;
    s_head = next_head;

    return true;
}

/*
 * Function:
 *     app_event_init
 *
 * Purpose:
 *     Initializes the ordered event queue and clears overflow accounting.
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
 *     Call before enabling interrupt producers.
 */
void app_event_init(void)
{
    s_head = 0u;
    s_tail = 0u;
    s_overflow_count = 0u;
}

/*
 * Function:
 *     app_event_post_rx_byte_from_isr
 *
 * Purpose:
 *     Posts one received UART byte to the ordered event queue.
 *
 * Input Parameters:
 *     data_byte:
 *         Received UART byte to preserve in the event.
 *
 * Output Parameters:
 *     None.
 *
 * Return Value:
 *     true:
 *         The event was queued.
 *     false:
 *         The queue was full and the event was rejected.
 *
 * Notes:
 *     Called from USART2 interrupt context and performs bounded work.
 */
bool app_event_post_rx_byte_from_isr(uint8_t data_byte)
{
    return app_event_post_from_isr(APP_EVENT_TYPE_UART_RX_BYTE, data_byte);
}

/*
 * Function:
 *     app_event_post_tick_from_isr
 *
 * Purpose:
 *     Posts one sample-timer event to the ordered event queue.
 *
 * Input Parameters:
 *     None.
 *
 * Output Parameters:
 *     None.
 *
 * Return Value:
 *     true:
 *         The event was queued.
 *     false:
 *         The queue was full and the event was rejected.
 *
 * Notes:
 *     Called from TIM6 interrupt context and performs bounded work.
 */
bool app_event_post_tick_from_isr(void)
{
    return app_event_post_from_isr(APP_EVENT_TYPE_SAMPLE_TICK, APP_EVENT_DATA_UNUSED);
}

/*
 * Function:
 *     app_event_post_uart_error_from_isr
 *
 * Purpose:
 *     Posts one UART-error observation to the ordered event queue.
 *
 * Input Parameters:
 *     None.
 *
 * Output Parameters:
 *     None.
 *
 * Return Value:
 *     true:
 *         The event was queued.
 *     false:
 *         The queue was full and the event was rejected.
 *
 * Notes:
 *     Called from USART2 interrupt context and performs bounded work.
 */
bool app_event_post_uart_error_from_isr(void)
{
    return app_event_post_from_isr(APP_EVENT_TYPE_UART_ERROR, APP_EVENT_DATA_UNUSED);
}

/*
 * Function:
 *     app_event_take
 *
 * Purpose:
 *     Retrieves the oldest queued event without blocking.
 *
 * Input Parameters:
 *     event:
 *         Pointer to caller-owned storage that shall receive the event.
 *
 * Output Parameters:
 *     event:
 *         Receives the oldest event when the function returns true.
 *
 * Return Value:
 *     true:
 *         One event was retrieved.
 *     false:
 *         The pointer was NULL or the queue was empty.
 *
 * Notes:
 *     Called from main context.
 */
bool app_event_take(app_event_t *event)
{
    uint32_t primask;

    if (event == NULL)
    {
        return false;
    }

    primask = app_event_enter_critical();
    if (s_tail == s_head)
    {
        app_event_leave_critical(primask);
        return false;
    }

    *event = s_queue[s_tail];
    s_tail = (uint16_t)((s_tail + 1u) & APP_EVENT_QUEUE_MASK);
    app_event_leave_critical(primask);

    return true;
}

/*
 * Function:
 *     app_event_get_overflow_count
 *
 * Purpose:
 *     Returns the saturated count of event-post attempts rejected because the
 *     queue was full.
 *
 * Input Parameters:
 *     None.
 *
 * Output Parameters:
 *     None.
 *
 * Return Value:
 *     The current saturated event-queue overflow count.
 */
uint32_t app_event_get_overflow_count(void)
{
    return s_overflow_count;
}

/*
 * Function:
 *     app_event_wait
 *
 * Purpose:
 *     Places the MCU in wait-for-interrupt state only when the event queue is empty.
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
 *     Called from main context. The host-test implementation performs no wait.
 */
void app_event_wait(void)
{
#ifndef APP_EVENT_HOST_TEST
    uint32_t primask;

    // Disable interrupts before checking the queue so an event cannot arrive
    // between the empty check and WFI. A newly pending interrupt wakes WFI even
    // while PRIMASK is set; the saved mask is restored immediately afterward.
    primask = app_event_enter_critical();
    if (s_tail == s_head)
    {
        __asm volatile("DSB" : : : "memory");
        __asm volatile("WFI" : : : "memory");
    }
    app_event_leave_critical(primask);
#endif
}
