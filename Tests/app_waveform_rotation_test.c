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

#define TEST_INTERVAL_US (50000u)
#define TICKS_PER_SEGMENT (200u)

static uint32_t s_telemetry_count;
static float s_latest_sample;

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
    assert(length >= PROTOCOL_FRAME_OVERHEAD_LENGTH);

    if (data[3] == PROTOCOL_MESSAGE_TELEMETRY_SAMPLE)
    {
        const uint32_t sample_bits = protocol_read_u32_le(&data[16]);

        s_telemetry_count += 1u;
        (void)memcpy(&s_latest_sample, &sample_bits, sizeof(s_latest_sample));
    }

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

static float absolute_value(float value)
{
    return (value < 0.0f) ? -value : value;
}

static void push_rx_byte(uint8_t data_byte)
{
    const app_event_t event = { APP_EVENT_TYPE_UART_RX_BYTE, data_byte };
    app_process_event(&event);
}

static void push_tick(void)
{
    const app_event_t event = { APP_EVENT_TYPE_SAMPLE_TICK, 0u };
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
    for (byte_index = 0u; byte_index < encoded_length; ++byte_index)
    {
        push_rx_byte(encoded[byte_index]);
    }
}

static void push_ticks(uint32_t tick_count)
{
    uint32_t tick_index;

    for (tick_index = 0u; tick_index < tick_count; ++tick_index)
    {
        push_tick();
    }
}

int main(void)
{
    uint8_t interval_payload[2];

    s_telemetry_count = 0u;
    s_latest_sample = 0.0f;
    app_init();

    protocol_write_u16_le(interval_payload, TEST_INTERVAL_US);
    send_command(PROTOCOL_MESSAGE_SET_STREAM_CONFIG,
                 1u,
                 interval_payload,
                 (uint16_t)sizeof(interval_payload));
    send_command(PROTOCOL_MESSAGE_START_STREAM, 2u, NULL, 0u);

    push_ticks(TICKS_PER_SEGMENT);
    assert(s_telemetry_count == TICKS_PER_SEGMENT);
    assert(s_latest_sample == 1.0f);

    push_ticks(TICKS_PER_SEGMENT);
    assert(s_telemetry_count == (TICKS_PER_SEGMENT * 2u));
    assert(absolute_value(s_latest_sample) < 0.0001f);

    push_ticks(TICKS_PER_SEGMENT);
    assert(s_telemetry_count == (TICKS_PER_SEGMENT * 3u));
    assert(absolute_value(s_latest_sample) < 0.0001f);

    push_ticks(3u);
    assert((s_latest_sample > 0.10f) && (s_latest_sample < 0.13f));

    push_ticks(TICKS_PER_SEGMENT - 3u);
    assert(s_telemetry_count == (TICKS_PER_SEGMENT * 4u));
    assert(absolute_value(s_latest_sample) < 0.0002f);

    push_tick();
    assert((s_latest_sample > 0.30f) && (s_latest_sample < 0.32f));

    return 0;
}
