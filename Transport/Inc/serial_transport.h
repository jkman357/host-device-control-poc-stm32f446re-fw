// Copyright (c) 2026 Ray Yang. All rights reserved.
//
// File:
//     serial_transport.h
//
// Purpose:
//     Defines the bounded USART2 transport-adapter interface.
//
// Public Contract:
//     - Initializes the configured USART2 Baud Rate profile.
//     - Queues bounded transmit data without blocking.
//     - Reports saturated receive, transmit, and UART error counters.
//     - Exposes the USART2 interrupt entry point required by the vector table.
//
// Notes:
//     The adapter posts received bytes and errors to the ordered application event queue.

#ifndef SERIAL_TRANSPORT_H
#define SERIAL_TRANSPORT_H

#include <stddef.h>
#include <stdint.h>

#include "serial_baud.h"

#define SERIAL_TRANSPORT_BITS_PER_BYTE SERIAL_BAUD_BITS_PER_BYTE
#define SERIAL_TRANSPORT_TX_RING_CAPACITY (2048u)

typedef enum
{
    SERIAL_TRANSPORT_RESULT_OK = 0u,
    SERIAL_TRANSPORT_RESULT_INVALID_ARGUMENT = 1u,
    SERIAL_TRANSPORT_RESULT_NO_CAPACITY = 2u
} serial_transport_result_t;

/*
 * Function:
 *     serial_transport_init
 *
 * Purpose:
 *     Initializes the transmit queue, counters, and USART2 hardware profile.
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
 *     Call once before enabling application processing.
 */
void serial_transport_init(void);
/*
 * Function:
 *     serial_transport_write
 *
 * Purpose:
 *     Queues a complete byte sequence for non-blocking USART2 transmission.
 *
 * Input Parameters:
 *     data:
 *         Pointer to bytes to queue.
 *     length:
 *         Number of bytes to queue.
 *
 * Output Parameters:
 *     None.
 *
 * Return Value:
 *     SERIAL_TRANSPORT_RESULT_OK:
 *         All bytes were queued.
 *     SERIAL_TRANSPORT_RESULT_INVALID_ARGUMENT:
 *         The data pointer was NULL for a nonzero length.
 *     SERIAL_TRANSPORT_RESULT_QUEUE_FULL:
 *         Available queue capacity was insufficient.
 *
 * Notes:
 *     Runs in main context and uses a bounded critical section.
 */
serial_transport_result_t serial_transport_write(
    const uint8_t *data,
    size_t length);
/*
 * Function:
 *     serial_transport_get_rx_overflow_count
 *
 * Purpose:
 *     Returns the saturated count of received bytes rejected by the event queue.
 *
 * Input Parameters:
 *     None.
 *
 * Output Parameters:
 *     None.
 *
 * Return Value:
 *     Current saturated receive-overflow count.
 */
uint32_t serial_transport_get_rx_overflow_count(void);
/*
 * Function:
 *     serial_transport_get_tx_overflow_count
 *
 * Purpose:
 *     Returns the saturated count of transmit writes rejected for insufficient capacity.
 *
 * Input Parameters:
 *     None.
 *
 * Output Parameters:
 *     None.
 *
 * Return Value:
 *     Current saturated transmit-overflow count.
 */
uint32_t serial_transport_get_tx_overflow_count(void);
/*
 * Function:
 *     serial_transport_get_uart_error_count
 *
 * Purpose:
 *     Returns the saturated count of observed USART2 hardware errors.
 *
 * Input Parameters:
 *     None.
 *
 * Output Parameters:
 *     None.
 *
 * Return Value:
 *     Current saturated UART-error count.
 */
uint32_t serial_transport_get_uart_error_count(void);
/*
 * Function:
 *     USART2_IRQHandler
 *
 * Purpose:
 *     Services bounded USART2 receive, error, and transmit-ready conditions.
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
 *     Executes in interrupt context and does not block or parse protocol frames.
 */
void USART2_IRQHandler(void);

#endif
