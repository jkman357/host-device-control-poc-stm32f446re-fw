// Copyright (c) 2026 Ray Yang. All rights reserved.

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "app.h"
#include "protocol.h"
#include "protocol_messages.h"
#include "serial_transport.h"

#define CAPTURE_CAPACITY (PROTOCOL_MAX_FRAME_LENGTH)
#define TEST_TELEMETRY_FRAME_BYTES (24u)
#define TEST_CALCULATED_MIN_INTERVAL_US \
    SERIAL_BAUD_CALCULATED_MIN_INTERVAL_US( \
        SERIAL_BAUD_PERIPHERAL_CLOCK_HZ, \
        SERIAL_TRANSPORT_BAUD_RATE, \
        TEST_TELEMETRY_FRAME_BYTES, \
        SERIAL_TRANSPORT_BITS_PER_BYTE, \
        SERIAL_BAUD_STREAM_RESERVE_PERCENT)
#define TEST_EFFECTIVE_MIN_INTERVAL_US \
    ((TEST_CALCULATED_MIN_INTERVAL_US > PROTOCOL_STREAM_INTERVAL_MIN_US) \
         ? TEST_CALCULATED_MIN_INTERVAL_US \
         : PROTOCOL_STREAM_INTERVAL_MIN_US)
#define TEST_STREAMING_SUPPORTED \
    ((SERIAL_BAUD_IS_COMMAND_ONLY(SERIAL_TRANSPORT_BAUD_RATE) == 0u) \
         ? 1u \
         : 0u)
#define TEST_MAX_STREAM_RATE_HZ \
    ((TEST_STREAMING_SUPPORTED != 0u) \
         ? (1000000u / TEST_EFFECTIVE_MIN_INTERVAL_US) \
         : 0u)

static uint8_t s_capture[CAPTURE_CAPACITY];
static size_t s_capture_length;

void platform_led_set(bool is_on)
{
    (void)is_on;
}

void platform_sample_timer_set_interval_us(uint16_t interval_us)
{
    (void)interval_us;
}

serial_transport_result_t serial_transport_write(const uint8_t *data,
                                                 size_t length)
{
    assert(data != NULL);
    assert(length <= sizeof(s_capture));
    (void)memcpy(s_capture, data, length);
    s_capture_length = length;
    return SERIAL_TRANSPORT_RESULT_OK;
}

uint32_t serial_transport_get_rx_overflow_count(void)
{
    return 0u;
}

uint32_t serial_transport_get_tx_overflow_count(void)
{
    return 0u;
}

uint32_t serial_transport_get_uart_error_count(void)
{
    return 0u;
}

static void push_rx_byte(uint8_t data_byte)
{
    const app_event_t event = { APP_EVENT_TYPE_UART_RX_BYTE, data_byte };
    app_process_event(&event);
}

static void send_command(uint8_t message_id,
                         uint16_t sequence,
                         const uint8_t *payload,
                         uint16_t payload_length)
{
    uint8_t encoded[PROTOCOL_MAX_FRAME_LENGTH];
    size_t encoded_length = 0u;
    size_t byte_index;
    const bool encoded_ok = protocol_encode_frame(message_id,
                                                   sequence,
                                                   payload,
                                                   payload_length,
                                                   encoded,
                                                   sizeof(encoded),
                                                   &encoded_length);

    assert(encoded_ok);
    s_capture_length = 0u;
    for (byte_index = 0u; byte_index < encoded_length; byte_index += 1u)
    {
        push_rx_byte(encoded[byte_index]);
    }
    assert(s_capture_length >= PROTOCOL_FRAME_OVERHEAD_LENGTH);
}

static void expect_nack(uint8_t request_id,
                        protocol_result_code_t result)
{
    assert(s_capture[3] == PROTOCOL_MESSAGE_NACK);
    assert(s_capture[8] == request_id);
    assert(s_capture[9] == (uint8_t)result);
    assert(s_capture[10] == 0u);
}

static void expect_ack(uint8_t request_id, uint8_t state)
{
    assert(s_capture[3] == PROTOCOL_MESSAGE_ACK);
    assert(s_capture[8] == request_id);
    assert(s_capture[9] == (uint8_t)PROTOCOL_RESULT_OK);
    assert(s_capture[10] == state);
}

int main(void)
{
    uint8_t interval_payload[2];
    uint16_t invalid_interval;

    app_init();

    send_command(PROTOCOL_MESSAGE_GET_DEVICE_INFO, 1u, NULL, 0u);
    assert(s_capture[3] == PROTOCOL_MESSAGE_DEVICE_INFO);
    assert(s_capture[12] == 7u);
    assert(protocol_read_u16_le(&s_capture[13])
           == (uint16_t)TEST_MAX_STREAM_RATE_HZ);

    if (TEST_STREAMING_SUPPORTED == 0u)
    {
        protocol_write_u16_le(interval_payload,
                              PROTOCOL_STREAM_INTERVAL_DEFAULT_US);
        send_command(PROTOCOL_MESSAGE_SET_STREAM_CONFIG,
                     2u,
                     interval_payload,
                     (uint16_t)sizeof(interval_payload));
        expect_nack(PROTOCOL_MESSAGE_SET_STREAM_CONFIG,
                    PROTOCOL_RESULT_INVALID_STATE);

        send_command(PROTOCOL_MESSAGE_START_STREAM, 3u, NULL, 0u);
        expect_nack(PROTOCOL_MESSAGE_START_STREAM,
                    PROTOCOL_RESULT_INVALID_STATE);
    }
    else
    {
        invalid_interval = (uint16_t)(TEST_EFFECTIVE_MIN_INTERVAL_US - 1u);
        protocol_write_u16_le(interval_payload, invalid_interval);
        send_command(PROTOCOL_MESSAGE_SET_STREAM_CONFIG,
                     2u,
                     interval_payload,
                     (uint16_t)sizeof(interval_payload));
        expect_nack(PROTOCOL_MESSAGE_SET_STREAM_CONFIG,
                    PROTOCOL_RESULT_INVALID_VALUE);

        protocol_write_u16_le(
            interval_payload,
            (uint16_t)TEST_EFFECTIVE_MIN_INTERVAL_US);
        send_command(PROTOCOL_MESSAGE_SET_STREAM_CONFIG,
                     3u,
                     interval_payload,
                     (uint16_t)sizeof(interval_payload));
        expect_ack(PROTOCOL_MESSAGE_SET_STREAM_CONFIG, 0u);

        send_command(PROTOCOL_MESSAGE_START_STREAM, 4u, NULL, 0u);
        expect_ack(PROTOCOL_MESSAGE_START_STREAM, 1u);
    }

    return 0;
}
