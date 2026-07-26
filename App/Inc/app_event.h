// Copyright (c) 2026 Ray Yang. All rights reserved.
//
// File:
//     app_event.h
//
// Purpose:
//     Defines the bounded ordered event-queue contract.
//
// Public Contract:
//     - Defines event types exchanged between interrupt and main contexts.
//     - Posts bounded events from interrupt context.
//     - Retrieves events in original production order from main context.
//
// Notes:
//     The queue has fixed capacity and never allocates memory dynamically.

#ifndef APP_EVENT_H
#define APP_EVENT_H

#include <stdbool.h>
#include <stdint.h>

#define APP_EVENT_QUEUE_CAPACITY (256u)

typedef enum
{
    APP_EVENT_TYPE_UART_RX_BYTE = 0u,
    APP_EVENT_TYPE_SAMPLE_TICK = 1u,
    APP_EVENT_TYPE_UART_ERROR = 2u
} app_event_type_t;

typedef struct
{
    app_event_type_t type;
    uint8_t data_byte;
} app_event_t;

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
void app_event_init(void);
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
bool app_event_post_rx_byte_from_isr(uint8_t data_byte);
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
bool app_event_post_tick_from_isr(void);
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
bool app_event_post_uart_error_from_isr(void);
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
bool app_event_take(app_event_t *event);
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
uint32_t app_event_get_overflow_count(void);
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
void app_event_wait(void);

#endif
