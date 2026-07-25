// Copyright (c) 2026 Ray Yang. All rights reserved.
//
// File:
//     app.c
//
// Purpose:
//     Implements the device application controller for the firmware PoC.
//
// Responsibilities:
//     - Owns the authoritative idle and streaming state model.
//     - Processes shared-Protocol commands in main context.
//     - Builds ACK, NACK, DEVICE_INFO, and TELEMETRY_SAMPLE messages.
//     - Coordinates configurable timer, transport, parser, and sine source behavior.

#include "app.h"

#include <limits.h>
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
#define FW_VERSION_PATCH                       (3u)
#define MICROSECONDS_PER_SECOND                (1000000u)
#define LED_TOGGLE_PERIOD_US                   (500000u)

#define ACK_NACK_PAYLOAD_LENGTH                (3u)
#define ACK_NACK_REQUEST_ID_OFFSET             (0u)
#define ACK_NACK_RESULT_OFFSET                 (1u)
#define ACK_NACK_STATE_OFFSET                  (2u)

#define DEVICE_INFO_TYPE_OFFSET                (0u)
#define DEVICE_INFO_FW_MAJOR_OFFSET            (2u)
#define DEVICE_INFO_FW_MINOR_OFFSET            (3u)
#define DEVICE_INFO_FW_PATCH_OFFSET            (4u)
#define DEVICE_INFO_MAX_RATE_OFFSET            (5u)
#define DEVICE_INFO_NAME_LENGTH_OFFSET         (7u)
#define DEVICE_INFO_NAME_OFFSET                (8u)
#define DEVICE_INFO_FIXED_LENGTH               (8u)
#define DEVICE_INFO_MAX_RATE_HZ                \
    (MICROSECONDS_PER_SECOND / PROTOCOL_STREAM_INTERVAL_MIN_US)

#define SET_STREAM_CONFIG_PAYLOAD_LENGTH       (2u)
#define SET_STREAM_CONFIG_INTERVAL_OFFSET      (0u)

#define TELEMETRY_PAYLOAD_LENGTH               (14u)
#define TELEMETRY_SAMPLE_COUNTER_OFFSET        (0u)
#define TELEMETRY_DEVICE_TICK_OFFSET           (4u)
#define TELEMETRY_SINE_VALUE_OFFSET            (8u)
#define TELEMETRY_STATUS_FLAGS_OFFSET          (12u)

static const uint8_t s_device_name[] =
{
    'N', 'U', 'C', 'L', 'E', 'O', '-', 'F', '4', '4', '6', 'R', 'E'
};

_Static_assert(sizeof(s_device_name) <= PROTOCOL_DEVICE_NAME_MAX_LENGTH,
               "Device name exceeds Protocol limit.");

static device_state_t s_device_state;
static protocol_parser_t s_protocol_parser;
static protocol_frame_t s_received_frame;
static uint8_t s_encoded_frame[PROTOCOL_MAX_FRAME_LENGTH];
static uint16_t s_stream_interval_us;
static uint16_t s_unsolicited_sequence;
static uint32_t s_device_tick_us;
static uint32_t s_stream_phase_us;
static uint32_t s_sample_counter;
static uint32_t s_led_elapsed_us;
static uint32_t s_send_failure_count;
static uint32_t s_event_overflow_count;
static uint32_t s_parser_timeout_observed_count;
static bool s_is_led_on;
static bool s_uart_error_event_seen;

/*
 * Function:
 *     app_increment_saturating_u32
 *
 * Purpose:
 *     Increments a diagnostic counter without unsigned wraparound.
 *
 * Input Parameters:
 *     counter:
 *         Points to the module-owned counter to update.
 *
 * Output Parameters:
 *     counter:
 *         Receives the incremented value or remains UINT32_MAX.
 *
 * Return Value:
 *     None.
 */
static void app_increment_saturating_u32(uint32_t *counter)
{
    if (*counter < UINT32_MAX)
    {
        *counter += 1u;
    }
}

/*
 * Function:
 *     app_add_modulo_u32
 *
 * Purpose:
 *     Adds an increment with explicit modulo-2^32 behavior.
 *
 * Input Parameters:
 *     value:
 *         Points to the counter to update.
 *     increment:
 *         Supplies the increment.
 *
 * Output Parameters:
 *     value:
 *         Receives the modulo-2^32 result.
 *
 * Return Value:
 *     None.
 */
static void app_add_modulo_u32(uint32_t *value, uint32_t increment)
{
    uint64_t sum;

    sum = (uint64_t)(*value) + (uint64_t)increment;
    *value = (uint32_t)sum;
}

/*
 * Function:
 *     app_transition_to
 *
 * Purpose:
 *     Applies one authoritative application-state transition and entry actions.
 *
 * Input Parameters:
 *     next_state:
 *         Supplies the destination device state.
 *
 * Output Parameters:
 *     None.
 *
 * Return Value:
 *     true:
 *         The requested state was valid and applied.
 *     false:
 *         The requested state was outside the Protocol state model.
 *
 * Notes:
 *     Runs only in main context and is the sole writer of s_device_state.
 */
static bool app_transition_to(device_state_t next_state)
{
    if ((next_state != DEVICE_STATE_IDLE) && (next_state != DEVICE_STATE_STREAMING))
    {
        return false;
    }

    if (next_state == DEVICE_STATE_IDLE)
    {
        s_is_led_on = false;
        s_led_elapsed_us = 0u;
        platform_led_set(false);
    }
    else
    {
        s_unsolicited_sequence = 1u;
        s_sample_counter = 0u;
        s_stream_phase_us = 0u;
        s_is_led_on = false;
        s_led_elapsed_us = 0u;
        platform_led_set(false);
    }

    s_device_state = next_state;
    return true;
}

/*
 * Function:
 *     app_take_unsolicited_sequence
 *
 * Purpose:
 *     Returns and advances the independent unsolicited-message sequence.
 *
 * Input Parameters:
 *     None.
 *
 * Output Parameters:
 *     None.
 *
 * Return Value:
 *     result:
 *         Current sequence before modulo-65536 advancement.
 */
static uint16_t app_take_unsolicited_sequence(void)
{
    uint16_t sequence;

    sequence = s_unsolicited_sequence;
    if (s_unsolicited_sequence == UINT16_MAX)
    {
        s_unsolicited_sequence = 0u;
    }
    else
    {
        s_unsolicited_sequence += 1u;
    }

    return sequence;
}

/*
 * Function:
 *     app_take_next_sample_counter
 *
 * Purpose:
 *     Advances and returns the modulo-2^32 telemetry sample counter.
 *
 * Input Parameters:
 *     None.
 *
 * Output Parameters:
 *     None.
 *
 * Return Value:
 *     result:
 *         Updated sample counter. The first sample after START_STREAM is one.
 */
static uint32_t app_take_next_sample_counter(void)
{
    app_add_modulo_u32(&s_sample_counter, 1u);
    return s_sample_counter;
}

/*
 * Function:
 *     app_advance_stream_phase
 *
 * Purpose:
 *     Advances the one-second sine phase by one configured sample interval.
 *
 * Input Parameters:
 *     interval_us:
 *         Supplies the elapsed sample interval.
 *
 * Output Parameters:
 *     None.
 *
 * Return Value:
 *     result:
 *         Updated phase in the range zero through 999999 microseconds.
 */
static uint32_t app_advance_stream_phase(uint16_t interval_us)
{
    s_stream_phase_us += (uint32_t)interval_us;
    if (s_stream_phase_us >= MICROSECONDS_PER_SECOND)
    {
        s_stream_phase_us -= MICROSECONDS_PER_SECOND;
    }

    return s_stream_phase_us;
}

/*
 * Function:
 *     app_send_frame
 *
 * Purpose:
 *     Encodes and queues one bounded shared-Protocol frame.
 *
 * Input Parameters:
 *     message_id:
 *         Supplies the message identifier.
 *     sequence:
 *         Supplies the frame sequence.
 *     payload:
 *         Points to payload bytes or is NULL for an empty payload.
 *     payload_length:
 *         Supplies the payload length.
 *
 * Output Parameters:
 *     None.
 *
 * Return Value:
 *     true:
 *         The frame was encoded and queued.
 *     false:
 *         Encoding failed or the TX ring had insufficient capacity.
 *
 * Notes:
 *     Runs only in main context and owns s_encoded_frame during the call.
 */
static bool app_send_frame(uint8_t message_id,
                           uint16_t sequence,
                           const uint8_t *payload,
                           uint16_t payload_length)
{
    size_t encoded_length;
    bool is_encoded;
    serial_transport_result_t transport_result;

    is_encoded = protocol_encode_frame(message_id,
                                       sequence,
                                       payload,
                                       payload_length,
                                       s_encoded_frame,
                                       sizeof(s_encoded_frame),
                                       &encoded_length);
    if (is_encoded == false)
    {
        return false;
    }

    transport_result = serial_transport_write(s_encoded_frame, encoded_length);
    return (transport_result == SERIAL_TRANSPORT_RESULT_OK);
}

/*
 * Function:
 *     app_record_send_result
 *
 * Purpose:
 *     Records a failed frame-queue attempt without blocking or retrying.
 *
 * Input Parameters:
 *     is_sent:
 *         Supplies the app_send_frame result.
 *
 * Output Parameters:
 *     None.
 *
 * Return Value:
 *     None.
 */
static void app_record_send_result(bool is_sent)
{
    if (is_sent == false)
    {
        app_increment_saturating_u32(&s_send_failure_count);
    }
}

/*
 * Function:
 *     app_send_ack
 *
 * Purpose:
 *     Sends a successful direct response using the request sequence.
 *
 * Input Parameters:
 *     request_message_id:
 *         Supplies the accepted command identifier.
 *     request_sequence:
 *         Supplies the request sequence to copy.
 *
 * Output Parameters:
 *     None.
 *
 * Return Value:
 *     None.
 */
static void app_send_ack(uint8_t request_message_id, uint16_t request_sequence)
{
    uint8_t payload[ACK_NACK_PAYLOAD_LENGTH];
    bool is_sent;

    payload[ACK_NACK_REQUEST_ID_OFFSET] = request_message_id;
    payload[ACK_NACK_RESULT_OFFSET] = (uint8_t)PROTOCOL_RESULT_OK;
    payload[ACK_NACK_STATE_OFFSET] = (uint8_t)s_device_state;

    is_sent = app_send_frame(PROTOCOL_MESSAGE_ACK,
                             request_sequence,
                             payload,
                             ACK_NACK_PAYLOAD_LENGTH);
    app_record_send_result(is_sent);
}

/*
 * Function:
 *     app_send_nack
 *
 * Purpose:
 *     Sends a rejected direct response using the request sequence.
 *
 * Input Parameters:
 *     request_message_id:
 *         Supplies the rejected command identifier.
 *     request_sequence:
 *         Supplies the request sequence to copy.
 *     result_code:
 *         Supplies the authoritative rejection result.
 *
 * Output Parameters:
 *     None.
 *
 * Return Value:
 *     None.
 */
static void app_send_nack(uint8_t request_message_id,
                          uint16_t request_sequence,
                          protocol_result_code_t result_code)
{
    uint8_t payload[ACK_NACK_PAYLOAD_LENGTH];
    bool is_sent;

    payload[ACK_NACK_REQUEST_ID_OFFSET] = request_message_id;
    payload[ACK_NACK_RESULT_OFFSET] = (uint8_t)result_code;
    payload[ACK_NACK_STATE_OFFSET] = (uint8_t)s_device_state;

    is_sent = app_send_frame(PROTOCOL_MESSAGE_NACK,
                             request_sequence,
                             payload,
                             ACK_NACK_PAYLOAD_LENGTH);
    app_record_send_result(is_sent);
}

/*
 * Function:
 *     app_send_device_info
 *
 * Purpose:
 *     Sends the authoritative DEVICE_INFO direct response.
 *
 * Input Parameters:
 *     request_sequence:
 *         Supplies the request sequence to copy.
 *
 * Output Parameters:
 *     None.
 *
 * Return Value:
 *     None.
 */
static void app_send_device_info(uint16_t request_sequence)
{
    uint8_t payload[DEVICE_INFO_FIXED_LENGTH + sizeof(s_device_name)];
    size_t name_index;
    bool is_sent;

    protocol_write_u16_le(&payload[DEVICE_INFO_TYPE_OFFSET],
                          PROTOCOL_DEVICE_TYPE_STM32F446RE);
    payload[DEVICE_INFO_FW_MAJOR_OFFSET] = FW_VERSION_MAJOR;
    payload[DEVICE_INFO_FW_MINOR_OFFSET] = FW_VERSION_MINOR;
    payload[DEVICE_INFO_FW_PATCH_OFFSET] = FW_VERSION_PATCH;
    protocol_write_u16_le(&payload[DEVICE_INFO_MAX_RATE_OFFSET],
                          (uint16_t)DEVICE_INFO_MAX_RATE_HZ);
    payload[DEVICE_INFO_NAME_LENGTH_OFFSET] = (uint8_t)sizeof(s_device_name);

    name_index = 0u;
    while (name_index < sizeof(s_device_name))
    {
        payload[DEVICE_INFO_NAME_OFFSET + name_index] = s_device_name[name_index];
        name_index += 1u;
    }

    is_sent = app_send_frame(PROTOCOL_MESSAGE_DEVICE_INFO,
                             request_sequence,
                             payload,
                             (uint16_t)sizeof(payload));
    app_record_send_result(is_sent);
}

/*
 * Function:
 *     app_get_status_flags
 *
 * Purpose:
 *     Builds the sticky transport status flags defined by the Protocol.
 *
 * Input Parameters:
 *     None.
 *
 * Output Parameters:
 *     None.
 *
 * Return Value:
 *     result:
 *         Bitwise OR of defined protocol_status_flag_t values.
 */
static uint16_t app_get_status_flags(void)
{
    serial_transport_statistics_t statistics;
    uint16_t status_flags;

    serial_transport_get_statistics(&statistics);
    status_flags = (uint16_t)PROTOCOL_STATUS_NONE;

    if (statistics.rx_overflow_count != 0u)
    {
        status_flags |= (uint16_t)PROTOCOL_STATUS_RX_OVERFLOW_OBSERVED;
    }
    if (statistics.tx_overflow_count != 0u)
    {
        status_flags |= (uint16_t)PROTOCOL_STATUS_TX_OVERFLOW_OBSERVED;
    }
    if ((statistics.uart_error_count != 0u) || (s_uart_error_event_seen == true))
    {
        status_flags |= (uint16_t)PROTOCOL_STATUS_UART_ERROR_OBSERVED;
    }

    return status_flags;
}

/*
 * Function:
 *     app_handle_ping
 *
 * Purpose:
 *     Validates PING and sends ACK or NACK.
 *
 * Input Parameters:
 *     request:
 *         Points to the decoded request.
 *
 * Output Parameters:
 *     None.
 *
 * Return Value:
 *     None.
 */
static void app_handle_ping(const protocol_frame_t *request)
{
    if (request->payload_length != 0u)
    {
        app_send_nack(request->message_id,
                      request->sequence,
                      PROTOCOL_RESULT_INVALID_LENGTH);
    }
    else
    {
        app_send_ack(request->message_id, request->sequence);
    }
}

/*
 * Function:
 *     app_handle_get_device_info
 *
 * Purpose:
 *     Validates GET_DEVICE_INFO and sends DEVICE_INFO or NACK.
 *
 * Input Parameters:
 *     request:
 *         Points to the decoded request.
 *
 * Output Parameters:
 *     None.
 *
 * Return Value:
 *     None.
 */
static void app_handle_get_device_info(const protocol_frame_t *request)
{
    if (request->payload_length != 0u)
    {
        app_send_nack(request->message_id,
                      request->sequence,
                      PROTOCOL_RESULT_INVALID_LENGTH);
    }
    else
    {
        app_send_device_info(request->sequence);
    }
}

/*
 * Function:
 *     app_handle_set_stream_config
 *
 * Purpose:
 *     Validates and applies SET_STREAM_CONFIG while idle.
 *
 * Input Parameters:
 *     request:
 *         Points to the decoded request.
 *
 * Output Parameters:
 *     None.
 *
 * Return Value:
 *     None.
 */
static void app_handle_set_stream_config(const protocol_frame_t *request)
{
    uint16_t requested_interval_us;
    bool is_applied;

    if (request->payload_length != SET_STREAM_CONFIG_PAYLOAD_LENGTH)
    {
        app_send_nack(request->message_id,
                      request->sequence,
                      PROTOCOL_RESULT_INVALID_LENGTH);
        return;
    }

    if (s_device_state != DEVICE_STATE_IDLE)
    {
        app_send_nack(request->message_id,
                      request->sequence,
                      PROTOCOL_RESULT_INVALID_STATE);
        return;
    }

    requested_interval_us =
        protocol_read_u16_le(&request->payload[SET_STREAM_CONFIG_INTERVAL_OFFSET]);
    if ((requested_interval_us < PROTOCOL_STREAM_INTERVAL_MIN_US) ||
        (requested_interval_us > PROTOCOL_STREAM_INTERVAL_MAX_US))
    {
        app_send_nack(request->message_id,
                      request->sequence,
                      PROTOCOL_RESULT_INVALID_VALUE);
        return;
    }

    is_applied = platform_set_sample_period_us(requested_interval_us);
    if (is_applied == false)
    {
        app_send_nack(request->message_id,
                      request->sequence,
                      PROTOCOL_RESULT_INTERNAL_ERROR);
        return;
    }

    s_stream_interval_us = requested_interval_us;
    app_send_ack(request->message_id, request->sequence);
}

/*
 * Function:
 *     app_handle_start_stream
 *
 * Purpose:
 *     Validates START_STREAM and enters streaming on success.
 *
 * Input Parameters:
 *     request:
 *         Points to the decoded request.
 *
 * Output Parameters:
 *     None.
 *
 * Return Value:
 *     None.
 */
static void app_handle_start_stream(const protocol_frame_t *request)
{
    bool is_transitioned;

    if (request->payload_length != 0u)
    {
        app_send_nack(request->message_id,
                      request->sequence,
                      PROTOCOL_RESULT_INVALID_LENGTH);
        return;
    }

    if (s_device_state != DEVICE_STATE_IDLE)
    {
        app_send_nack(request->message_id,
                      request->sequence,
                      PROTOCOL_RESULT_INVALID_STATE);
        return;
    }

    is_transitioned = app_transition_to(DEVICE_STATE_STREAMING);
    if (is_transitioned == false)
    {
        app_send_nack(request->message_id,
                      request->sequence,
                      PROTOCOL_RESULT_INTERNAL_ERROR);
        return;
    }

    app_send_ack(request->message_id, request->sequence);
}

/*
 * Function:
 *     app_handle_stop_stream
 *
 * Purpose:
 *     Validates STOP_STREAM and enters idle on success.
 *
 * Input Parameters:
 *     request:
 *         Points to the decoded request.
 *
 * Output Parameters:
 *     None.
 *
 * Return Value:
 *     None.
 */
static void app_handle_stop_stream(const protocol_frame_t *request)
{
    bool is_transitioned;

    if (request->payload_length != 0u)
    {
        app_send_nack(request->message_id,
                      request->sequence,
                      PROTOCOL_RESULT_INVALID_LENGTH);
        return;
    }

    if (s_device_state != DEVICE_STATE_STREAMING)
    {
        app_send_nack(request->message_id,
                      request->sequence,
                      PROTOCOL_RESULT_INVALID_STATE);
        return;
    }

    is_transitioned = app_transition_to(DEVICE_STATE_IDLE);
    if (is_transitioned == false)
    {
        app_send_nack(request->message_id,
                      request->sequence,
                      PROTOCOL_RESULT_INTERNAL_ERROR);
        return;
    }

    app_send_ack(request->message_id, request->sequence);
}

/*
 * Function:
 *     app_handle_request
 *
 * Purpose:
 *     Dispatches one CRC-valid request according to the authoritative contract.
 *
 * Input Parameters:
 *     request:
 *         Points to the decoded frame.
 *
 * Output Parameters:
 *     None.
 *
 * Return Value:
 *     None.
 */
static void app_handle_request(const protocol_frame_t *request)
{
    if (request->version != PROTOCOL_VERSION)
    {
        app_send_nack(request->message_id,
                      request->sequence,
                      PROTOCOL_RESULT_UNSUPPORTED_VERSION);
        return;
    }

    if (request->sequence == 0u)
    {
        app_send_nack(request->message_id,
                      request->sequence,
                      PROTOCOL_RESULT_INVALID_VALUE);
        return;
    }

    switch (request->message_id)
    {
        case PROTOCOL_MESSAGE_PING:
            app_handle_ping(request);
            break;

        case PROTOCOL_MESSAGE_GET_DEVICE_INFO:
            app_handle_get_device_info(request);
            break;

        case PROTOCOL_MESSAGE_SET_STREAM_CONFIG:
            app_handle_set_stream_config(request);
            break;

        case PROTOCOL_MESSAGE_START_STREAM:
            app_handle_start_stream(request);
            break;

        case PROTOCOL_MESSAGE_STOP_STREAM:
            app_handle_stop_stream(request);
            break;

        default:
            app_send_nack(request->message_id,
                          request->sequence,
                          PROTOCOL_RESULT_INVALID_COMMAND);
            break;
    }
}

/*
 * Function:
 *     app_process_received_bytes
 *
 * Purpose:
 *     Drains received bytes and dispatches complete CRC-valid frames.
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
static void app_process_received_bytes(void)
{
    uint8_t data_byte;
    protocol_parse_result_t parse_result;

    while (serial_transport_read_byte(&data_byte) == true)
    {
        parse_result = protocol_parser_push_byte(&s_protocol_parser,
                                                 data_byte,
                                                 &s_received_frame);
        if (parse_result == PROTOCOL_PARSE_FRAME_READY)
        {
            app_handle_request(&s_received_frame);
        }
    }
}

/*
 * Function:
 *     app_update_led
 *
 * Purpose:
 *     Updates the streaming heartbeat using elapsed microseconds.
 *
 * Input Parameters:
 *     elapsed_us:
 *         Supplies elapsed timer time.
 *
 * Output Parameters:
 *     None.
 *
 * Return Value:
 *     None.
 */
static void app_update_led(uint16_t elapsed_us)
{
    if (s_device_state != DEVICE_STATE_STREAMING)
    {
        s_led_elapsed_us = 0u;
        s_is_led_on = false;
        platform_led_set(false);
        return;
    }

    s_led_elapsed_us += (uint32_t)elapsed_us;
    if (s_led_elapsed_us >= LED_TOGGLE_PERIOD_US)
    {
        s_led_elapsed_us -= LED_TOGGLE_PERIOD_US;
        s_is_led_on = (s_is_led_on == false);
        platform_led_set(s_is_led_on);
    }
}

/*
 * Function:
 *     app_send_telemetry
 *
 * Purpose:
 *     Builds and queues one authoritative TELEMETRY_SAMPLE event.
 *
 * Input Parameters:
 *     interval_us:
 *         Supplies the elapsed configured sample interval.
 *
 * Output Parameters:
 *     None.
 *
 * Return Value:
 *     None.
 */
static void app_send_telemetry(uint16_t interval_us)
{
    uint8_t payload[TELEMETRY_PAYLOAD_LENGTH];
    uint32_t sample_counter;
    uint32_t phase_us;
    uint16_t status_flags;
    uint16_t sequence;
    float sine_value;
    bool is_sent;

    sample_counter = app_take_next_sample_counter();
    phase_us = app_advance_stream_phase(interval_us);
    sine_value = sine_generator_get_sample(phase_us);
    status_flags = app_get_status_flags();
    sequence = app_take_unsolicited_sequence();

    protocol_write_u32_le(&payload[TELEMETRY_SAMPLE_COUNTER_OFFSET], sample_counter);
    protocol_write_u32_le(&payload[TELEMETRY_DEVICE_TICK_OFFSET], s_device_tick_us);
    protocol_write_float32_le(&payload[TELEMETRY_SINE_VALUE_OFFSET], sine_value);
    protocol_write_u16_le(&payload[TELEMETRY_STATUS_FLAGS_OFFSET], status_flags);

    is_sent = app_send_frame(PROTOCOL_MESSAGE_TELEMETRY_SAMPLE,
                             sequence,
                             payload,
                             TELEMETRY_PAYLOAD_LENGTH);
    app_record_send_result(is_sent);
}

/*
 * Function:
 *     app_process_one_tick
 *
 * Purpose:
 *     Processes one timer interval in main context.
 *
 * Input Parameters:
 *     interval_us:
 *         Supplies the period represented by the pending tick.
 *
 * Output Parameters:
 *     None.
 *
 * Return Value:
 *     None.
 */
static void app_process_one_tick(uint16_t interval_us)
{
    bool did_timeout;

    app_add_modulo_u32(&s_device_tick_us, (uint32_t)interval_us);
    did_timeout = protocol_parser_advance_time_us(&s_protocol_parser,
                                                  (uint32_t)interval_us);
    if (did_timeout == true)
    {
        app_increment_saturating_u32(&s_parser_timeout_observed_count);
    }

    app_update_led(interval_us);
    if (s_device_state == DEVICE_STATE_STREAMING)
    {
        app_send_telemetry(interval_us);
    }
}

/*
 * Function:
 *     app_init
 *
 * Purpose:
 *     Initializes application state and shared-Protocol processing.
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
void app_init(void)
{
    bool is_transitioned;

    s_stream_interval_us = PROTOCOL_STREAM_INTERVAL_DEFAULT_US;
    s_unsolicited_sequence = 1u;
    s_device_tick_us = 0u;
    s_stream_phase_us = 0u;
    s_sample_counter = 0u;
    s_led_elapsed_us = 0u;
    s_send_failure_count = 0u;
    s_event_overflow_count = 0u;
    s_parser_timeout_observed_count = 0u;
    s_is_led_on = false;
    s_uart_error_event_seen = false;

    protocol_parser_init(&s_protocol_parser);
    is_transitioned = app_transition_to(DEVICE_STATE_IDLE);
    if (is_transitioned == false)
    {
        platform_led_set(false);
    }
}

/*
 * Function:
 *     app_process_events
 *
 * Purpose:
 *     Processes one coherent event batch in main context.
 *
 * Input Parameters:
 *     event_batch:
 *         Points to the caller-owned event batch.
 *
 * Output Parameters:
 *     None.
 *
 * Return Value:
 *     None.
 */
void app_process_events(const app_event_batch_t *event_batch)
{
    uint16_t processed_tick_count;
    uint16_t batch_interval_us;

    if (event_batch == NULL)
    {
        return;
    }

    batch_interval_us = s_stream_interval_us;
    s_event_overflow_count = event_batch->tick_overflow_count;

    if ((event_batch->flags & APP_EVENT_FLAG_UART_ERROR) != 0u)
    {
        s_uart_error_event_seen = true;
    }

    if ((event_batch->flags & APP_EVENT_FLAG_UART_RX_AVAILABLE) != 0u)
    {
        app_process_received_bytes();
    }

    processed_tick_count = 0u;
    while (processed_tick_count < event_batch->tick_count)
    {
        app_process_one_tick(batch_interval_us);
        processed_tick_count += 1u;
    }
}
