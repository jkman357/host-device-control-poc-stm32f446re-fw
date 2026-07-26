// Copyright (c) 2026 Ray Yang. All rights reserved.
//
// File:
//     app_waveform_rotation_test.c
//
// Purpose:
//     Verifies application waveform rotation during streaming.
//
// Responsibilities:
//     - Stubs platform and transport dependencies.
//     - Advances application ticks through the four waveform intervals.
//     - Checks waveform samples and restart behavior.
//
// Notes:
//     This host test uses assertions and does not execute on the MCU target.

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "app.h"
#include "protocol.h"
#include "protocol_messages.h"
#include "serial_transport.h"

#define TEST_INTERVAL_US (50000u)
#define TICKS_PER_SEGMENT (200u)

static uint32_t s_telemetry_count;
static float s_latest_sample;

/*
 * Function:
 *     platform_led_set
 *
 * Purpose:
 *     Provides the test stub for the platform LED interface.
 *
 * Input Parameters:
 *     is_on:
 *         Requested LED state; the host test intentionally ignores it.
 *
 * Output Parameters:
 *     None.
 *
 * Return Value:
 *     None.
 *
 * Notes:
 *     This definition replaces the target platform adapter in the host test.
 */
void platform_led_set(bool is_on)
{
    (void)is_on;
}

/*
 * Function:
 *     platform_sample_timer_set_interval_us
 *
 * Purpose:
 *     Provides the test stub for the platform sample-timer configuration.
 *
 * Input Parameters:
 *     interval_us:
 *         Requested interval; the host test intentionally ignores it.
 *
 * Output Parameters:
 *     None.
 *
 * Return Value:
 *     None.
 *
 * Notes:
 *     This definition replaces the target platform adapter in the host test.
 */
void platform_sample_timer_set_interval_us(uint16_t interval_us)
{
    (void)interval_us;
}

/*
 * Function:
 *     serial_transport_write
 *
 * Purpose:
 *     Captures an encoded frame for host-test assertions.
 *
 * Input Parameters:
 *     data:
 *         Pointer to the encoded frame bytes.
 *     length:
 *         Number of valid encoded bytes.
 *
 * Output Parameters:
 *     None.
 *
 * Return Value:
 *     SERIAL_TRANSPORT_RESULT_OK:
 *         The test capture accepted the frame.
 *
 * Notes:
 *     This definition replaces the target serial transport in the host test.
 */
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

/*
 * Function:
 *     serial_transport_get_rx_overflow_count
 *
 * Purpose:
 *     Provides a zero-valued transport diagnostic counter for the host test.
 *
 * Input Parameters:
 *     None.
 *
 * Output Parameters:
 *     None.
 *
 * Return Value:
 *     Zero because this test does not inject the corresponding fault.
 *
 * Notes:
 *     This definition replaces the target serial transport in the host test.
 */
uint32_t serial_transport_get_rx_overflow_count(void)
{
    return 0u;
}

/*
 * Function:
 *     serial_transport_get_tx_overflow_count
 *
 * Purpose:
 *     Provides a zero-valued transport diagnostic counter for the host test.
 *
 * Input Parameters:
 *     None.
 *
 * Output Parameters:
 *     None.
 *
 * Return Value:
 *     Zero because this test does not inject the corresponding fault.
 *
 * Notes:
 *     This definition replaces the target serial transport in the host test.
 */
uint32_t serial_transport_get_tx_overflow_count(void)
{
    return 0u;
}

/*
 * Function:
 *     serial_transport_get_uart_error_count
 *
 * Purpose:
 *     Provides a zero-valued transport diagnostic counter for the host test.
 *
 * Input Parameters:
 *     None.
 *
 * Output Parameters:
 *     None.
 *
 * Return Value:
 *     Zero because this test does not inject the corresponding fault.
 *
 * Notes:
 *     This definition replaces the target serial transport in the host test.
 */
uint32_t serial_transport_get_uart_error_count(void)
{
    return 0u;
}

/*
 * Function:
 *     app_waveform_test_absolute_value
 *
 * Purpose:
 *     Returns the magnitude of one floating-point test value.
 *
 * Input Parameters:
 *     value:
 *         Value whose magnitude is requested.
 *
 * Output Parameters:
 *     None.
 *
 * Return Value:
 *     Absolute magnitude of value.
 */
static float app_waveform_test_absolute_value(float value)
{
    return (value < 0.0f) ? -value : value;
}

/*
 * Function:
 *     app_waveform_test_push_rx_byte
 *
 * Purpose:
 *     Delivers one command byte to the application through an ordered event.
 *
 * Input Parameters:
 *     data_byte:
 *         Byte to deliver.
 *
 * Output Parameters:
 *     None.
 *
 * Return Value:
 *     None.
 */
static void app_waveform_test_push_rx_byte(uint8_t data_byte)
{
    const app_event_t event = { APP_EVENT_TYPE_UART_RX_BYTE, data_byte };
    app_process_event(&event);
}

/*
 * Function:
 *     app_waveform_test_push_tick
 *
 * Purpose:
 *     Delivers one sample-tick event to the application.
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
static void app_waveform_test_push_tick(void)
{
    const app_event_t event = { APP_EVENT_TYPE_SAMPLE_TICK, 0u };
    app_process_event(&event);
}

/*
 * Function:
 *     app_waveform_test_send_command
 *
 * Purpose:
 *     Encodes and delivers one application command.
 *
 * Input Parameters:
 *     message_id:
 *         Command message identifier.
 *     sequence:
 *         Command sequence number.
 *     payload:
 *         Pointer to command payload, or NULL for zero length.
 *     payload_length:
 *         Number of payload bytes.
 *
 * Output Parameters:
 *     None.
 *
 * Return Value:
 *     None.
 */
static void app_waveform_test_send_command(uint8_t message_id,
                         uint16_t sequence,
                         const uint8_t *payload,
                         uint16_t payload_length)
{
    uint8_t encoded[PROTOCOL_MAX_FRAME_LENGTH];
    size_t encoded_length = 0u;
    size_t byte_index;
    const bool is_encoded = protocol_encode_frame(message_id,
                                                   sequence,
                                                   payload,
                                                   payload_length,
                                                   encoded,
                                                   sizeof(encoded),
                                                   &encoded_length);

    assert(is_encoded == true);
    for (byte_index = 0u; byte_index < encoded_length; ++byte_index)
    {
        app_waveform_test_push_rx_byte(encoded[byte_index]);
    }
}

/*
 * Function:
 *     app_waveform_test_push_ticks
 *
 * Purpose:
 *     Delivers a bounded number of consecutive sample-tick events.
 *
 * Input Parameters:
 *     tick_count:
 *         Number of events to deliver.
 *
 * Output Parameters:
 *     None.
 *
 * Return Value:
 *     None.
 */
static void app_waveform_test_push_ticks(uint32_t tick_count)
{
    uint32_t tick_index;

    for (tick_index = 0u; tick_index < tick_count; ++tick_index)
    {
        app_waveform_test_push_tick();
    }
}

/*
 * Function:
 *     main
 *
 * Purpose:
 *     Executes the application waveform-rotation host test.
 *
 * Input Parameters:
 *     None.
 *
 * Output Parameters:
 *     None.
 *
 * Return Value:
 *     EXIT_SUCCESS when all assertions pass.
 *
 * Notes:
 *     Assertion failure terminates the host process.
 */
int main(void)
{
    uint8_t interval_payload[2];

    s_telemetry_count = 0u;
    s_latest_sample = 0.0f;
    app_init();

    protocol_write_u16_le(interval_payload, TEST_INTERVAL_US);
    app_waveform_test_send_command(PROTOCOL_MESSAGE_SET_STREAM_CONFIG,
                 1u,
                 interval_payload,
                 (uint16_t)sizeof(interval_payload));
    app_waveform_test_send_command(PROTOCOL_MESSAGE_START_STREAM, 2u, NULL, 0u);

    app_waveform_test_push_ticks(TICKS_PER_SEGMENT);
    assert(s_telemetry_count == TICKS_PER_SEGMENT);
    assert(s_latest_sample == 1.0f);

    app_waveform_test_push_ticks(TICKS_PER_SEGMENT);
    assert(s_telemetry_count == (TICKS_PER_SEGMENT * 2u));
    assert(app_waveform_test_absolute_value(s_latest_sample) < 0.0001f);

    app_waveform_test_push_ticks(TICKS_PER_SEGMENT);
    assert(s_telemetry_count == (TICKS_PER_SEGMENT * 3u));
    assert(app_waveform_test_absolute_value(s_latest_sample) < 0.0001f);

    app_waveform_test_push_ticks(3u);
    assert((s_latest_sample > 0.10f) && (s_latest_sample < 0.13f));

    app_waveform_test_push_ticks(TICKS_PER_SEGMENT - 3u);
    assert(s_telemetry_count == (TICKS_PER_SEGMENT * 4u));
    assert(app_waveform_test_absolute_value(s_latest_sample) < 0.0002f);

    app_waveform_test_push_tick();
    assert((s_latest_sample > 0.30f) && (s_latest_sample < 0.32f));

    return EXIT_SUCCESS;
}
