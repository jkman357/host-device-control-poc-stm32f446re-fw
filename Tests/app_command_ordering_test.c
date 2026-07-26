// Copyright (c) 2026 Ray Yang. All rights reserved.

#include <assert.h>
#include <stddef.h>
#include <stdint.h>

#include "app.h"
#include "protocol.h"
#include "protocol_messages.h"
#include "serial_transport.h"

#define CAPTURE_CAPACITY (16u)

static uint8_t s_message_ids[CAPTURE_CAPACITY];
static uint32_t s_sample_counters[CAPTURE_CAPACITY];
static size_t s_capture_count;

void platform_led_set(bool is_on)
{
    (void)is_on;
}

void platform_sample_timer_set_interval_us(uint16_t interval_us)
{
    (void)interval_us;
}

serial_transport_result_t serial_transport_write(const uint8_t *data, size_t length)
{
    assert(data != NULL);
    assert(length >= PROTOCOL_FRAME_OVERHEAD_LENGTH);
    assert(s_capture_count < CAPTURE_CAPACITY);

    s_message_ids[s_capture_count] = data[3];
    if (data[3] == PROTOCOL_MESSAGE_TELEMETRY_SAMPLE)
    {
        s_sample_counters[s_capture_count] = protocol_read_u32_le(&data[8]);
    }
    else
    {
        s_sample_counters[s_capture_count] = 0u;
    }
    s_capture_count += 1u;
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
    app_event_t event = { APP_EVENT_TYPE_UART_RX_BYTE, data_byte };
    app_process_event(&event);
}

static void push_tick(void)
{
    const app_event_t event = { APP_EVENT_TYPE_SAMPLE_TICK, 0u };
    app_process_event(&event);
}

static size_t encode_command(uint8_t message_id,
                             uint16_t sequence,
                             uint8_t *output,
                             size_t capacity)
{
    size_t encoded_length = 0u;
    const bool encoded = protocol_encode_frame(message_id,
                                                sequence,
                                                NULL,
                                                0u,
                                                output,
                                                capacity,
                                                &encoded_length);
    assert(encoded);
    return encoded_length;
}

int main(void)
{
    uint8_t start_frame[PROTOCOL_FRAME_OVERHEAD_LENGTH];
    uint8_t stop_frame[PROTOCOL_FRAME_OVERHEAD_LENGTH];
    size_t start_length;
    size_t stop_length;
    size_t byte_index;

    s_capture_count = 0u;
    app_init();

    start_length = encode_command(PROTOCOL_MESSAGE_START_STREAM,
                                  1u,
                                  start_frame,
                                  sizeof(start_frame));
    for (byte_index = 0u; byte_index < (start_length - 1u); ++byte_index)
    {
        push_rx_byte(start_frame[byte_index]);
    }

    push_tick();
    assert(s_capture_count == 0u);

    push_rx_byte(start_frame[start_length - 1u]);
    assert(s_capture_count == 1u);
    assert(s_message_ids[0] == PROTOCOL_MESSAGE_ACK);

    push_tick();
    assert(s_capture_count == 2u);
    assert(s_message_ids[1] == PROTOCOL_MESSAGE_TELEMETRY_SAMPLE);
    assert(s_sample_counters[1] == 1u);

    stop_length = encode_command(PROTOCOL_MESSAGE_STOP_STREAM,
                                 2u,
                                 stop_frame,
                                 sizeof(stop_frame));
    for (byte_index = 0u; byte_index < (stop_length - 1u); ++byte_index)
    {
        push_rx_byte(stop_frame[byte_index]);
    }

    push_tick();
    assert(s_capture_count == 3u);
    assert(s_message_ids[2] == PROTOCOL_MESSAGE_TELEMETRY_SAMPLE);
    assert(s_sample_counters[2] == 2u);

    push_rx_byte(stop_frame[stop_length - 1u]);
    assert(s_capture_count == 4u);
    assert(s_message_ids[3] == PROTOCOL_MESSAGE_ACK);

    push_tick();
    assert(s_capture_count == 4u);
    return 0;
}
