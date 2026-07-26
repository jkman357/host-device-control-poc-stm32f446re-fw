// Copyright (c) 2026 Ray Yang. All rights reserved.

#include "app.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "device_state.h"
#include "platform.h"
#include "protocol.h"
#include "protocol_messages.h"
#include "serial_transport.h"
#include "sine_generator.h"

#define FW_VERSION_MAJOR                       (0u)
#define FW_VERSION_MINOR                       (2u)
#define FW_VERSION_PATCH                       (5u)
#define MICROSECONDS_PER_SECOND                (1000000u)
#define TELEMETRY_PAYLOAD_LENGTH               (14u)
#define TELEMETRY_FRAME_LENGTH                 \
    (PROTOCOL_FRAME_OVERHEAD_LENGTH + TELEMETRY_PAYLOAD_LENGTH)
#define TELEMETRY_WIRE_BITS_PER_SECOND         \
    ((MICROSECONDS_PER_SECOND / PROTOCOL_STREAM_INTERVAL_MIN_US) \
     * TELEMETRY_FRAME_LENGTH \
     * SERIAL_TRANSPORT_BITS_PER_BYTE)
#define UART_CAPACITY_BITS_PER_SECOND          (SERIAL_TRANSPORT_BAUD_RATE)
#define UART_RESERVED_PERCENT                  (20u)
#define UART_REQUIRED_WITH_RESERVE             \
    ((TELEMETRY_WIRE_BITS_PER_SECOND * (100u + UART_RESERVED_PERCENT)) / 100u)

_Static_assert(UART_CAPACITY_BITS_PER_SECOND >= UART_REQUIRED_WITH_RESERVE,
               "UART baud is insufficient for 1 kHz telemetry plus reserve.");

static device_state_t s_state;
static protocol_parser_t s_parser;
static protocol_frame_t s_frame;
static uint8_t s_encoded[PROTOCOL_MAX_FRAME_LENGTH];
static uint16_t s_interval_us;
static uint16_t s_unsolicited_sequence;
static uint32_t s_tick_us;
static uint32_t s_phase_us;
static uint32_t s_sample_counter;
static bool s_uart_error_seen;

static const uint8_t s_name[] =
{
    'N', 'U', 'C', 'L', 'E', 'O', '-', 'F', '4', '4', '6', 'R', 'E'
};

static bool app_send_frame(uint8_t message_id,
                           uint16_t sequence,
                           const uint8_t *payload,
                           uint16_t payload_length)
{
    size_t encoded_length;
    bool encoded;

    encoded = protocol_encode_frame(message_id,
                                    sequence,
                                    payload,
                                    payload_length,
                                    s_encoded,
                                    sizeof(s_encoded),
                                    &encoded_length);
    if (!encoded)
    {
        return false;
    }

    return serial_transport_write(s_encoded, encoded_length)
        == SERIAL_TRANSPORT_RESULT_OK;
}

static void app_send_ack(uint8_t request_id, uint16_t sequence)
{
    const uint8_t payload[3] =
    {
        request_id,
        (uint8_t)PROTOCOL_RESULT_OK,
        (uint8_t)s_state
    };

    (void)app_send_frame(PROTOCOL_MESSAGE_ACK,
                         sequence,
                         payload,
                         (uint16_t)sizeof(payload));
}

static void app_send_nack(uint8_t request_id,
                          uint16_t sequence,
                          protocol_result_code_t result)
{
    const uint8_t payload[3] =
    {
        request_id,
        (uint8_t)result,
        (uint8_t)s_state
    };

    (void)app_send_frame(PROTOCOL_MESSAGE_NACK,
                         sequence,
                         payload,
                         (uint16_t)sizeof(payload));
}

static void app_send_device_info(uint16_t sequence)
{
    uint8_t payload[8u + sizeof(s_name)];
    size_t name_index;

    protocol_write_u16_le(&payload[0], PROTOCOL_DEVICE_TYPE_STM32F446RE);
    payload[2] = FW_VERSION_MAJOR;
    payload[3] = FW_VERSION_MINOR;
    payload[4] = FW_VERSION_PATCH;
    protocol_write_u16_le(
        &payload[5],
        (uint16_t)(MICROSECONDS_PER_SECOND / PROTOCOL_STREAM_INTERVAL_MIN_US));
    payload[7] = (uint8_t)sizeof(s_name);

    for (name_index = 0u; name_index < sizeof(s_name); ++name_index)
    {
        payload[8u + name_index] = s_name[name_index];
    }

    (void)app_send_frame(PROTOCOL_MESSAGE_DEVICE_INFO,
                         sequence,
                         payload,
                         (uint16_t)sizeof(payload));
}

static void app_process_command(const protocol_frame_t *frame)
{
    uint16_t interval_us;

    if (frame->version != PROTOCOL_VERSION)
    {
        app_send_nack(frame->message_id,
                      frame->sequence,
                      PROTOCOL_RESULT_UNSUPPORTED_VERSION);
        return;
    }

    if (frame->sequence == 0u)
    {
        app_send_nack(frame->message_id,
                      frame->sequence,
                      PROTOCOL_RESULT_INVALID_VALUE);
        return;
    }

    switch (frame->message_id)
    {
        case PROTOCOL_MESSAGE_PING:
            if (frame->payload_length != 0u)
            {
                app_send_nack(frame->message_id,
                              frame->sequence,
                              PROTOCOL_RESULT_INVALID_LENGTH);
            }
            else
            {
                app_send_ack(frame->message_id, frame->sequence);
            }
            break;

        case PROTOCOL_MESSAGE_GET_DEVICE_INFO:
            if (frame->payload_length != 0u)
            {
                app_send_nack(frame->message_id,
                              frame->sequence,
                              PROTOCOL_RESULT_INVALID_LENGTH);
            }
            else
            {
                app_send_device_info(frame->sequence);
            }
            break;

        case PROTOCOL_MESSAGE_SET_STREAM_CONFIG:
            if (s_state != DEVICE_STATE_IDLE)
            {
                app_send_nack(frame->message_id,
                              frame->sequence,
                              PROTOCOL_RESULT_INVALID_STATE);
                break;
            }
            if (frame->payload_length != 2u)
            {
                app_send_nack(frame->message_id,
                              frame->sequence,
                              PROTOCOL_RESULT_INVALID_LENGTH);
                break;
            }

            interval_us = protocol_read_u16_le(frame->payload);
            if ((interval_us < PROTOCOL_STREAM_INTERVAL_MIN_US)
                || (interval_us > PROTOCOL_STREAM_INTERVAL_MAX_US))
            {
                app_send_nack(frame->message_id,
                              frame->sequence,
                              PROTOCOL_RESULT_INVALID_VALUE);
                break;
            }

            s_interval_us = interval_us;
            platform_sample_timer_set_interval_us(interval_us);
            app_send_ack(frame->message_id, frame->sequence);
            break;

        case PROTOCOL_MESSAGE_START_STREAM:
            if (frame->payload_length != 0u)
            {
                app_send_nack(frame->message_id,
                              frame->sequence,
                              PROTOCOL_RESULT_INVALID_LENGTH);
            }
            else if (s_state != DEVICE_STATE_IDLE)
            {
                app_send_nack(frame->message_id,
                              frame->sequence,
                              PROTOCOL_RESULT_INVALID_STATE);
            }
            else
            {
                s_state = DEVICE_STATE_STREAMING;
                s_unsolicited_sequence = 1u;
                s_sample_counter = 0u;
                s_phase_us = 0u;
                app_send_ack(frame->message_id, frame->sequence);
            }
            break;

        case PROTOCOL_MESSAGE_STOP_STREAM:
            if (frame->payload_length != 0u)
            {
                app_send_nack(frame->message_id,
                              frame->sequence,
                              PROTOCOL_RESULT_INVALID_LENGTH);
            }
            else if (s_state != DEVICE_STATE_STREAMING)
            {
                app_send_nack(frame->message_id,
                              frame->sequence,
                              PROTOCOL_RESULT_INVALID_STATE);
            }
            else
            {
                s_state = DEVICE_STATE_IDLE;
                platform_led_set(false);
                app_send_ack(frame->message_id, frame->sequence);
            }
            break;

        default:
            app_send_nack(frame->message_id,
                          frame->sequence,
                          PROTOCOL_RESULT_INVALID_COMMAND);
            break;
    }
}

static void app_process_rx_byte(uint8_t data_byte)
{
    const protocol_parse_result_t result =
        protocol_parser_push_byte(&s_parser, data_byte, &s_frame);

    if (result == PROTOCOL_PARSE_FRAME_READY)
    {
        app_process_command(&s_frame);
    }
}

static void app_process_tick(void)
{
    uint8_t payload[TELEMETRY_PAYLOAD_LENGTH];
    uint16_t status_flags = 0u;

    s_tick_us += s_interval_us;
    (void)protocol_parser_advance_time_us(&s_parser, s_interval_us);

    if (s_state != DEVICE_STATE_STREAMING)
    {
        return;
    }

    s_sample_counter += 1u;
    s_phase_us += s_interval_us;
    if (s_phase_us >= MICROSECONDS_PER_SECOND)
    {
        s_phase_us -= MICROSECONDS_PER_SECOND;
    }

    if (serial_transport_get_rx_overflow_count() != 0u)
    {
        status_flags |= PROTOCOL_STATUS_RX_OVERFLOW_OBSERVED;
    }
    if (serial_transport_get_tx_overflow_count() != 0u)
    {
        status_flags |= PROTOCOL_STATUS_TX_OVERFLOW_OBSERVED;
    }
    if (s_uart_error_seen || (serial_transport_get_uart_error_count() != 0u))
    {
        status_flags |= PROTOCOL_STATUS_UART_ERROR_OBSERVED;
    }

    protocol_write_u32_le(&payload[0], s_sample_counter);
    protocol_write_u32_le(&payload[4], s_tick_us);
    protocol_write_float32_le(&payload[8], sine_generator_sample(s_phase_us));
    protocol_write_u16_le(&payload[12], status_flags);

    (void)app_send_frame(PROTOCOL_MESSAGE_TELEMETRY_SAMPLE,
                         s_unsolicited_sequence,
                         payload,
                         TELEMETRY_PAYLOAD_LENGTH);
    s_unsolicited_sequence = (uint16_t)(s_unsolicited_sequence + 1u);
    platform_led_set((s_sample_counter & 0x7Fu) == 0u);
}

void app_init(void)
{
    s_state = DEVICE_STATE_IDLE;
    s_interval_us = PROTOCOL_STREAM_INTERVAL_DEFAULT_US;
    s_unsolicited_sequence = 1u;
    s_tick_us = 0u;
    s_phase_us = 0u;
    s_sample_counter = 0u;
    s_uart_error_seen = false;
    protocol_parser_init(&s_parser);
    platform_sample_timer_set_interval_us(s_interval_us);
}

void app_process_event(const app_event_t *event)
{
    if (event == NULL)
    {
        return;
    }

    switch (event->type)
    {
        case APP_EVENT_TYPE_UART_RX_BYTE:
            app_process_rx_byte(event->data_byte);
            break;

        case APP_EVENT_TYPE_SAMPLE_TICK:
            app_process_tick();
            break;

        case APP_EVENT_TYPE_UART_ERROR:
            s_uart_error_seen = true;
            break;

        default:
            break;
    }
}
