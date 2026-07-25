// Copyright (c) 2026 Ray Yang. All rights reserved.
//
// File:
//     serial_transport.h
//
// Purpose:
//     Defines the public USART2 transport contract.
//
// Public Contract:
//     - Initializes interrupt-driven USART2 transport.
//     - Transfers bytes through fixed-capacity RX and TX rings.
//     - Reports explicit transport results and diagnostics.

#ifndef SERIAL_TRANSPORT_H
#define SERIAL_TRANSPORT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    SERIAL_TRANSPORT_RESULT_OK = 0u,
    SERIAL_TRANSPORT_RESULT_INVALID_ARGUMENT = 1u,
    SERIAL_TRANSPORT_RESULT_NO_CAPACITY = 2u
} serial_transport_result_t;

typedef struct
{
    uint32_t rx_overflow_count;
    uint32_t tx_overflow_count;
    uint32_t uart_error_count;
} serial_transport_statistics_t;

/*
 * Function:
 *     serial_transport_init
 *
 * Purpose:
 *     Initialize USART2 and static RX/TX ring buffers.
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
void serial_transport_init(void);

/*
 * Function:
 *     serial_transport_read_byte
 *
 * Purpose:
 *     Removes one byte from the receive ring buffer.
 *
 * Input Parameters:
 *     None.
 *
 * Output Parameters:
 *     data_byte:
 *         Receives one byte when true is returned. The object remains
 *         unchanged when false is returned.
 *
 * Return Value:
 *     true:
 *         One byte was returned.
 *     false:
 *         The pointer was NULL or the receive ring was empty.
 *
 * Notes:
 *     Runs in main context and does not block.
 */
bool serial_transport_read_byte(uint8_t *data_byte);

/*
 * Function:
 *     serial_transport_write
 *
 * Purpose:
 *     Queues a contiguous byte sequence for interrupt-driven
 *         transmission.
 *
 * Input Parameters:
 *     data:
 *         Points to the bytes to queue. The function copies all bytes
 *         before returning.
 *     length:
 *         Supplies the number of bytes to queue.
 *
 * Output Parameters:
 *     None.
 *
 * Return Value:
 *     SERIAL_TRANSPORT_RESULT_OK:
 *         All bytes were queued.
 *     SERIAL_TRANSPORT_RESULT_INVALID_ARGUMENT:
 *         The pointer or length was invalid.
 *     SERIAL_TRANSPORT_RESULT_NO_CAPACITY:
 *         The transmit ring did not have sufficient capacity.
 *
 * Notes:
 *     Runs in main context and uses a bounded PRIMASK critical section.
 */
serial_transport_result_t serial_transport_write(const uint8_t *data, size_t length);

/*
 * Function:
 *     serial_transport_get_statistics
 *
 * Purpose:
 *     Returns an atomic snapshot of transport diagnostic counters.
 *
 * Input Parameters:
 *     None.
 *
 * Output Parameters:
 *     statistics:
 *         Receives the counter snapshot when the pointer is valid. A NULL
 *         pointer is ignored.
 *
 * Return Value:
 *     None.
 *
 * Notes:
 *     Runs in main context and uses a bounded PRIMASK critical section.
 */
void serial_transport_get_statistics(serial_transport_statistics_t *statistics);

/*
 * Function:
 *     serial_transport_usart2_irq_handler
 *
 * Purpose:
 *     Handles USART2 receive, transmit, and error interrupts.
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
 *     Execution Context: ISR. Blocking: prohibited. Reentrant: not required.
 *     Timing Budget: one received byte and one transmitted byte per call.
 */
void serial_transport_usart2_irq_handler(void);

#ifdef __cplusplus
}
#endif

#endif // SERIAL_TRANSPORT_H
