// Copyright (c) 2026 Ray Yang. All rights reserved.
//
// File:
//     app_command_ordering_test.c
//
// Purpose:
//     Verifies command and timer-event ordering at application boundaries.
//
// Responsibilities:
//     - Stubs platform and transport dependencies.
//     - Places timer events immediately before and after command-frame completion.
//     - Checks START and STOP streaming behavior against ordered event delivery.
//
// Notes:
//     This host test uses assertions and does not execute on the MCU target.

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include "app.h"
#include "protocol.h"
#include "protocol_messages.h"
#include "serial_transport.h"

#define CAPTURE_CAPACITY (16u)

static uint8_t s_message_ids[CAPTURE_CAPACITY];
static uint32_t s_sample_counters[CAPTURE_CAPACITY];
static size_t s_capture_count;

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
 *     app_command_test_push_rx_byte
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
static void app_command_test_push_rx_byte(uint8_t data_byte)
{
    app_event_t event = { APP_EVENT_TYPE_UART_RX_BYTE, data_byte };
    app_process_event(&event);
}

/*
 * Function:
 *     app_command_test_push_tick
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
static void app_command_test_push_tick(void)
{
    const app_event_t event = { APP_EVENT_TYPE_SAMPLE_TICK, 0u };
    app_process_event(&event);
}

/*
 * Function:
 *     app_command_test_encode_command
 *
 * Purpose:
 *     Encodes a zero-payload command into caller-owned storage.
 *
 * Input Parameters:
 *     message_id:
 *         Command message identifier.
 *     sequence:
 *         Command sequence number.
 *     output:
 *         Pointer to caller-owned output storage.
 *     capacity:
 *         Available output bytes.
 *
 * Output Parameters:
 *     output:
 *         Receives the encoded command.
 *
 * Return Value:
 *     Encoded command length in bytes.
 */
static size_t app_command_test_encode_command(uint8_t message_id,
                             uint16_t sequence,
                             uint8_t *output,
                             size_t capacity)
{
    size_t encoded_length = 0u;
    const bool is_encoded = protocol_encode_frame(message_id,
                                                sequence,
                                                NULL,
                                                0u,
                                                output,
                                                capacity,
                                                &encoded_length);
    assert(is_encoded == true);
    return encoded_length;
}

/*
 * Function:
 *     main
 *
 * Purpose:
 *     Executes the command-versus-tick ordering host test.
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
    uint8_t start_frame[PROTOCOL_FRAME_OVERHEAD_LENGTH];
    uint8_t stop_frame[PROTOCOL_FRAME_OVERHEAD_LENGTH];
    size_t start_length;
    size_t stop_length;
    size_t byte_index;

    s_capture_count = 0u;
    app_init();

    start_length = app_command_test_encode_command(PROTOCOL_MESSAGE_START_STREAM,
                                  1u,
                                  start_frame,
                                  sizeof(start_frame));
    for (byte_index = 0u; byte_index < (start_length - 1u); ++byte_index)
    {
        app_command_test_push_rx_byte(start_frame[byte_index]);
    }

    app_command_test_push_tick();
    assert(s_capture_count == 0u);

    app_command_test_push_rx_byte(start_frame[start_length - 1u]);
    assert(s_capture_count == 1u);
    assert(s_message_ids[0] == PROTOCOL_MESSAGE_ACK);

    app_command_test_push_tick();
    assert(s_capture_count == 2u);
    assert(s_message_ids[1] == PROTOCOL_MESSAGE_TELEMETRY_SAMPLE);
    assert(s_sample_counters[1] == 1u);

    stop_length = app_command_test_encode_command(PROTOCOL_MESSAGE_STOP_STREAM,
                                 2u,
                                 stop_frame,
                                 sizeof(stop_frame));
    for (byte_index = 0u; byte_index < (stop_length - 1u); ++byte_index)
    {
        app_command_test_push_rx_byte(stop_frame[byte_index]);
    }

    app_command_test_push_tick();
    assert(s_capture_count == 3u);
    assert(s_message_ids[2] == PROTOCOL_MESSAGE_TELEMETRY_SAMPLE);
    assert(s_sample_counters[2] == 2u);

    app_command_test_push_rx_byte(stop_frame[stop_length - 1u]);
    assert(s_capture_count == 4u);
    assert(s_message_ids[3] == PROTOCOL_MESSAGE_ACK);

    app_command_test_push_tick();
    assert(s_capture_count == 4u);
    return EXIT_SUCCESS;
}
