// Copyright (c) 2026 Ray Yang. All rights reserved.

#include "app_event.h"

#include <limits.h>
#include <stddef.h>

#define APP_EVENT_QUEUE_MASK (APP_EVENT_QUEUE_CAPACITY - 1u)

#if ((APP_EVENT_QUEUE_CAPACITY & APP_EVENT_QUEUE_MASK) != 0u)
#error APP_EVENT_QUEUE_CAPACITY must be a power of two.
#endif

static volatile uint16_t s_head;
static volatile uint16_t s_tail;
static volatile uint32_t s_overflow_count;
static app_event_t s_queue[APP_EVENT_QUEUE_CAPACITY];

#ifndef APP_EVENT_HOST_TEST
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

static void app_event_leave_critical(uint32_t primask)
{
    __asm volatile("MSR primask, %0" : : "r"(primask) : "memory");
}
#else
static uint32_t app_event_enter_critical(void)
{
    return 0u;
}

static void app_event_leave_critical(uint32_t primask)
{
    (void)primask;
}
#endif

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

void app_event_init(void)
{
    s_head = 0u;
    s_tail = 0u;
    s_overflow_count = 0u;
}

bool app_event_post_rx_byte_from_isr(uint8_t data_byte)
{
    return app_event_post_from_isr(APP_EVENT_TYPE_UART_RX_BYTE, data_byte);
}

bool app_event_post_tick_from_isr(void)
{
    return app_event_post_from_isr(APP_EVENT_TYPE_SAMPLE_TICK, 0u);
}

bool app_event_post_uart_error_from_isr(void)
{
    return app_event_post_from_isr(APP_EVENT_TYPE_UART_ERROR, 0u);
}

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

uint32_t app_event_get_overflow_count(void)
{
    return s_overflow_count;
}

void app_event_wait(void)
{
#ifndef APP_EVENT_HOST_TEST
    uint32_t primask;

    /*
     * Disable interrupts before checking the queue so an event cannot arrive
     * between the empty check and WFI. A newly pending interrupt wakes WFI even
     * while PRIMASK is set; the saved mask is restored immediately afterward.
     */
    primask = app_event_enter_critical();
    if (s_tail == s_head)
    {
        __asm volatile("DSB" : : : "memory");
        __asm volatile("WFI" : : : "memory");
    }
    app_event_leave_critical(primask);
#endif
}
