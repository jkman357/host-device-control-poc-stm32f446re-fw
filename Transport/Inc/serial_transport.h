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

/**
 * @brief Initialize USART2 and static RX/TX ring buffers.
 */
void SerialTransport_Init(void);

/**
 * @brief Read one byte from the RX ring buffer.
 * @param[out] data_byte Destination byte.
 * @return True when one byte was returned.
 */
bool SerialTransport_ReadByte(uint8_t *data_byte);

/**
 * @brief Queue a contiguous byte sequence for interrupt-driven transmission.
 * @param data Source bytes.
 * @param length Number of bytes.
 * @return Transport result.
 */
serial_transport_result_t SerialTransport_Write(const uint8_t *data, size_t length);

/**
 * @brief Return a snapshot of transport diagnostic counters.
 * @param[out] statistics Destination statistics.
 */
void SerialTransport_GetStatistics(serial_transport_statistics_t *statistics);

#ifdef __cplusplus
}
#endif

#endif /* SERIAL_TRANSPORT_H */
