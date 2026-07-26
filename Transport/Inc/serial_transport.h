// Copyright (c) 2026 Ray Yang. All rights reserved.

#ifndef SERIAL_TRANSPORT_H
#define SERIAL_TRANSPORT_H

#include <stddef.h>
#include <stdint.h>

#define SERIAL_TRANSPORT_BAUD_RATE (460800u)
#define SERIAL_TRANSPORT_BITS_PER_BYTE (10u)
#define SERIAL_TRANSPORT_TX_RING_CAPACITY (2048u)

typedef enum
{
    SERIAL_TRANSPORT_RESULT_OK = 0u,
    SERIAL_TRANSPORT_RESULT_INVALID_ARGUMENT = 1u,
    SERIAL_TRANSPORT_RESULT_NO_CAPACITY = 2u
} serial_transport_result_t;

void serial_transport_init(void);
serial_transport_result_t serial_transport_write(
    const uint8_t *data,
    size_t length);
uint32_t serial_transport_get_rx_overflow_count(void);
uint32_t serial_transport_get_tx_overflow_count(void);
uint32_t serial_transport_get_uart_error_count(void);
void USART2_IRQHandler(void);

#endif
