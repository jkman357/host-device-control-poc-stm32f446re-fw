// Copyright (c) 2026 Ray Yang. All rights reserved.
//
// File:
//     app.c
//
// Purpose:
//     Implements the device application controller for the firmware PoC.
//
// Responsibilities:
//     - Owns device state and command behavior.
//     - Processes protocol requests in main context.
//     - Builds bounded response and telemetry payloads.
//     - Coordinates the event, transport, platform, and sample-source modules.

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

#define FW_VERSION_MAJOR                    (0u)
#define FW_VERSION_MINOR                    (1u)
#define FW_VERSION_PATCH                    (5u)
#define BOARD_ID_NUCLEO_F446RE              (1u)
#define TRANSPORT_ID_USART2_STLINK_VCP      (1u)
#define SAMPLE_PERIOD_US                    (5000u)
#define DEVICE_CAPABILITY_STREAMING         (1u << 0u)
#define DEVICE_CAPABILITY_CRC16             (1u << 1u)
#define DEVICE_CAPABILITY_EVENT_DRIVEN      (1u << 2u)

#define PING_RESPONSE_LENGTH                (6u)
#define PING_RESPONSE_UPTIME_OFFSET         (0u)
#define PING_RESPONSE_STATE_OFFSET          (4u)
#define PING_RESPONSE_VERSION_OFFSET        (5u)

#define DEVICE_INFO_RESPONSE_LENGTH         (12u)
#define DEVICE_INFO_PROTOCOL_OFFSET         (0u)
#define DEVICE_INFO_FW_MAJOR_OFFSET         (1u)
#define DEVICE_INFO_FW_MINOR_OFFSET         (2u)
#define DEVICE_INFO_FW_PATCH_OFFSET         (3u)
#define DEVICE_INFO_BOARD_OFFSET            (4u)
#define DEVICE_INFO_TRANSPORT_OFFSET        (5u)
#define DEVICE_INFO_SAMPLE_PERIOD_OFFSET    (6u)
#define DEVICE_INFO_MAX_PAYLOAD_OFFSET      (8u)
#define DEVICE_INFO_CAPABILITIES_OFFSET     (9u)
#define DEVICE_INFO_RESERVED_0_OFFSET       (10u)
#define DEVICE_INFO_RESERVED_1_OFFSET       (11u)

#define ERROR_RESPONSE_LENGTH               (4u)
#define ERROR_RESPONSE_MESSAGE_ID_OFFSET    (0u)
#define ERROR_RESPONSE_RESULT_OFFSET        (2u)
#define ERROR_RESPONSE_STATE_OFFSET         (3u)

#define CONTROL_RESPONSE_LENGTH             (2u)
#define CONTROL_RESPONSE_RESULT_OFFSET      (0u)
#define CONTROL_RESPONSE_STATE_OFFSET       (1u)

#define TELEMETRY_PAYLOAD_LENGTH            (14u)
#define TELEMETRY_UPTIME_OFFSET             (0u)
#define TELEMETRY_SAMPLE_OFFSET             (4u)
#define TELEMETRY_STATE_OFFSET              (6u)
#define TELEMETRY_STATUS_OFFSET             (7u)
#define TELEMETRY_EVENT_OVERFLOW_OFFSET     (8u)
#define TELEMETRY_RX_OVERFLOW_OFFSET        (10u)
#define TELEMETRY_TX_OVERFLOW_OFFSET        (12u)

#define LED_TOGGLE_TICK_COUNT               (100u)

#define TELEMETRY_STATUS_STREAMING          (1u << 0u)
#define TELEMETRY_STATUS_UART_ERROR         (1u << 1u)
#define TELEMETRY_STATUS_EVENT_OVERFLOW     (1u << 2u)
#define TELEMETRY_STATUS_TX_REJECTED        (1u << 3u)

static device_state_t s_device_state;
static protocol_parser_t s_protocol_parser;
static uint32_t s_uptime_ms;
static uint16_t s_telemetry_sequence;
static uint16_t s_led_tick_count;
static bool s_is_led_on;
static bool s_has_uart_error;
static uint32_t s_last_event_overflow_count;
static uint32_t s_tx_reject_count;

/*
 * Function:
 *     app_increment_saturating_u32
 *
 * Purpose:
 *     Increments a diagnostic counter without allowing unsigned wraparound.
 *
 * Input Parameters:
 *     counter:
 *         Points to the module-owned counter to update.
 *
 * Output Parameters:
 *     counter:
 *         Receives the incremented value or remains UINT32_MAX when saturated.
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
 *     app_transition_to
 *
 * Purpose:
 *     Applies one explicit application-state transition and its entry actions.
 *
 * Input Parameters:
 *     next_state:
 *         Supplies the destination device state.
 *
 * Output Parameters:
 *     None.
 *
 * Return Value:
 *     None.
 *
 * Notes:
 *     Runs only in main context. This function is the sole writer of s_device_state.
 */
static void app_transition_to(device_state_t next_state)
{
    switch (next_state)
    {
        case DEVICE_STATE_IDLE:
            s_is_led_on = false;
            s_led_tick_count = 0u;
            platform_led_set(false);
            break;

        case DEVICE_STATE_STREAMING:
            s_telemetry_sequence = 0u;
            s_led_tick_count = 0u;
            s_is_led_on = false;
            sine_generator_reset();
            break;

        case DEVICE_STATE_FAULT:
            s_is_led_on = false;
            s_led_tick_count = 0u;
            platform_led_set(false);
            break;

        default:
            next_state = DEVICE_STATE_FAULT;
            s_is_led_on = false;
            s_led_tick_count = 0u;
            platform_led_set(false);
            break;
    }

    s_device_state = next_state;
}

/*
 * Function:
 *     app_add_uptime
 *
 * Purpose:
 *     Add elapsed milliseconds without wrapping uptime.
 *
 * Input Parameters:
 *     elapsed_ms:
 *         Elapsed time.
 *
 * Output Parameters:
 *     None.
 *
 * Return Value:
 *     None.
 */
static void app_add_uptime(uint32_t elapsed_ms)
{
    if (s_uptime_ms <= (UINT32_MAX - elapsed_ms))
    {
        s_uptime_ms += elapsed_ms;
    }
    else
    {
        s_uptime_ms = UINT32_MAX;
    }
}

/*
 * Function:
 *     app_saturate_to_u16
 *
 * Purpose:
 *     Return a 16-bit saturation of a 32-bit diagnostic counter.
 *
 * Input Parameters:
 *     value:
 *         Source value.
 *
 * Output Parameters:
 *     None.
 *
 * Return Value:
 *     result:
 *         Saturated value.
 */
static uint16_t app_saturate_to_u16(uint32_t value)
{
    if (value > UINT16_MAX)
    {
        return UINT16_MAX;
    }

    return (uint16_t)value;
}

/*
 * Function:
 *     app_take_telemetry_sequence
 *
 * Purpose:
 *     Advance the protocol telemetry sequence modulo 65536. Intentional
 *         wraparound is isolated here because the wire-format sequence is
 *         defined as a modulo-65536 counter. The returned value
 *         identifies the sample attempt, including attempts dropped
 *         because the TX queue had no capacity.
 *
 * Input Parameters:
 *     None.
 *
 * Output Parameters:
 *     None.
 *
 * Return Value:
 *     result:
 *         Current sequence before advancing.
 */
static uint16_t app_take_telemetry_sequence(void)
{
    uint16_t sequence;

    sequence = s_telemetry_sequence;

    if (s_telemetry_sequence == UINT16_MAX)
    {
        s_telemetry_sequence = 0u;
    }
    else
    {
        s_telemetry_sequence += 1u;
    }

    return sequence;
}

/*
 * Function:
 *     app_send_frame
 *
 * Purpose:
 *     Encodes and queues one bounded protocol frame.
 *
 * Input Parameters:
 *     message_id:
 *         Supplies the message identifier.
 *     flags:
 *         Supplies the frame flags.
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
 *         Encoding failed or the transport had insufficient capacity.
 *
 * Notes:
 *     Runs in main context and does not retry a rejected transmit.
 */
static bool app_send_frame(uint16_t message_id,
                          uint8_t flags,
                          uint16_t sequence,
                          const uint8_t *payload,
                          uint8_t payload_length)
{
    uint8_t encoded_frame[PROTOCOL_MAX_FRAME_LENGTH];
    size_t encoded_length;
    bool is_encoded;
    serial_transport_result_t transport_result;

    is_encoded = protocol_encode_frame(message_id,
                                       flags,
                                       sequence,
                                       payload,
                                       payload_length,
                                       encoded_frame,
                                       sizeof(encoded_frame),
                                       &encoded_length);

    if (is_encoded == false)
    {
        return false;
    }

    transport_result = serial_transport_write(encoded_frame, encoded_length);
    return (transport_result == SERIAL_TRANSPORT_RESULT_OK);
}

/*
 * Function:
 *     app_send_error_response
 *
 * Purpose:
 *     Send a generic error response for a rejected request.
 *
 * Input Parameters:
 *     request_message_id:
 *         Rejected request message identifier.
 *     request_sequence:
 *         Request sequence to echo.
 *     result_code:
 *         Rejection result code.
 *
 * Output Parameters:
 *     None.
 *
 * Return Value:
 *     None.
 */
static void app_send_error_response(uint16_t request_message_id,
                                    uint16_t request_sequence,
                                    protocol_command_result_t result_code)
{
    uint8_t payload[ERROR_RESPONSE_LENGTH];

    protocol_write_u16_le(&payload[ERROR_RESPONSE_MESSAGE_ID_OFFSET], request_message_id);
    payload[ERROR_RESPONSE_RESULT_OFFSET] = (uint8_t)result_code;
    payload[ERROR_RESPONSE_STATE_OFFSET] = (uint8_t)s_device_state;

    if (app_send_frame(PROTOCOL_MESSAGE_ERROR_RESPONSE,
                       PROTOCOL_FLAG_RESPONSE,
                       request_sequence,
                       payload,
                       ERROR_RESPONSE_LENGTH) == false)
    {
        app_increment_saturating_u32(&s_tx_reject_count);
    }
}

/*
 * Function:
 *     app_send_control_response
 *
 * Purpose:
 *     Send a dedicated start/stop control response.
 *
 * Input Parameters:
 *     response_message_id:
 *         Dedicated response identifier.
 *     request_sequence:
 *         Request sequence to echo.
 *     result_code:
 *         Command result code.
 *
 * Output Parameters:
 *     None.
 *
 * Return Value:
 *     None.
 */
static void app_send_control_response(uint16_t response_message_id,
                                      uint16_t request_sequence,
                                      protocol_command_result_t result_code)
{
    uint8_t payload[CONTROL_RESPONSE_LENGTH];

    payload[CONTROL_RESPONSE_RESULT_OFFSET] = (uint8_t)result_code;
    payload[CONTROL_RESPONSE_STATE_OFFSET] = (uint8_t)s_device_state;

    if (app_send_frame(response_message_id,
                       PROTOCOL_FLAG_RESPONSE,
                       request_sequence,
                       payload,
                       CONTROL_RESPONSE_LENGTH) == false)
    {
        app_increment_saturating_u32(&s_tx_reject_count);
    }
}

/*
 * Function:
 *     app_handle_ping_request
 *
 * Purpose:
 *     Process a validated PING request.
 *
 * Input Parameters:
 *     request:
 *         Request frame.
 *
 * Output Parameters:
 *     None.
 *
 * Return Value:
 *     None.
 */
static void app_handle_ping_request(const protocol_frame_t *request)
{
    uint8_t payload[PING_RESPONSE_LENGTH];

    if (request->payload_length != 0u)
    {
        app_send_error_response(request->message_id,
                                request->sequence,
                              PROTOCOL_COMMAND_RESULT_INVALID_PAYLOAD);
        return;
    }

    protocol_write_u32_le(&payload[PING_RESPONSE_UPTIME_OFFSET], s_uptime_ms);
    payload[PING_RESPONSE_STATE_OFFSET] = (uint8_t)s_device_state;
    payload[PING_RESPONSE_VERSION_OFFSET] = PROTOCOL_VERSION;

    if (app_send_frame(PROTOCOL_MESSAGE_PING_RESPONSE,
                       PROTOCOL_FLAG_RESPONSE,
                       request->sequence,
                       payload,
                       PING_RESPONSE_LENGTH) == false)
    {
        app_increment_saturating_u32(&s_tx_reject_count);
    }
}

/*
 * Function:
 *     app_handle_device_info_request
 *
 * Purpose:
 *     Process a validated GET_DEVICE_INFO request.
 *
 * Input Parameters:
 *     request:
 *         Request frame.
 *
 * Output Parameters:
 *     None.
 *
 * Return Value:
 *     None.
 */
static void app_handle_device_info_request(const protocol_frame_t *request)
{
    uint8_t payload[DEVICE_INFO_RESPONSE_LENGTH];
    const uint8_t capabilities = (uint8_t)(DEVICE_CAPABILITY_STREAMING |
                                            DEVICE_CAPABILITY_CRC16 |
                                            DEVICE_CAPABILITY_EVENT_DRIVEN);

    if (request->payload_length != 0u)
    {
        app_send_error_response(request->message_id,
                                request->sequence,
                              PROTOCOL_COMMAND_RESULT_INVALID_PAYLOAD);
        return;
    }

    payload[DEVICE_INFO_PROTOCOL_OFFSET] = PROTOCOL_VERSION;
    payload[DEVICE_INFO_FW_MAJOR_OFFSET] = FW_VERSION_MAJOR;
    payload[DEVICE_INFO_FW_MINOR_OFFSET] = FW_VERSION_MINOR;
    payload[DEVICE_INFO_FW_PATCH_OFFSET] = FW_VERSION_PATCH;
    payload[DEVICE_INFO_BOARD_OFFSET] = BOARD_ID_NUCLEO_F446RE;
    payload[DEVICE_INFO_TRANSPORT_OFFSET] = TRANSPORT_ID_USART2_STLINK_VCP;
    protocol_write_u16_le(&payload[DEVICE_INFO_SAMPLE_PERIOD_OFFSET], SAMPLE_PERIOD_US);
    payload[DEVICE_INFO_MAX_PAYLOAD_OFFSET] = PROTOCOL_MAX_PAYLOAD_LENGTH;
    payload[DEVICE_INFO_CAPABILITIES_OFFSET] = capabilities;
    payload[DEVICE_INFO_RESERVED_0_OFFSET] = 0u;
    payload[DEVICE_INFO_RESERVED_1_OFFSET] = 0u;

    if (app_send_frame(PROTOCOL_MESSAGE_DEVICE_INFO_RESPONSE,
                       PROTOCOL_FLAG_RESPONSE,
                       request->sequence,
                       payload,
                       DEVICE_INFO_RESPONSE_LENGTH) == false)
    {
        app_increment_saturating_u32(&s_tx_reject_count);
    }
}

/*
 * Function:
 *     app_handle_start_stream_request
 *
 * Purpose:
 *     Process a START_STREAM request.
 *
 * Input Parameters:
 *     request:
 *         Request frame.
 *
 * Output Parameters:
 *     None.
 *
 * Return Value:
 *     None.
 */
static void app_handle_start_stream_request(const protocol_frame_t *request)
{
    protocol_command_result_t result_code;

    if (request->payload_length != 0u)
    {
        result_code = PROTOCOL_COMMAND_RESULT_INVALID_PAYLOAD;
    }
    else if (s_device_state == DEVICE_STATE_IDLE)
    {
        app_transition_to(DEVICE_STATE_STREAMING);
        result_code = PROTOCOL_COMMAND_RESULT_OK;
    }
    else
    {
        result_code = PROTOCOL_COMMAND_RESULT_INVALID_STATE;
    }

    app_send_control_response(PROTOCOL_MESSAGE_START_STREAM_RESPONSE,
                              request->sequence,
                            result_code);
}

/*
 * Function:
 *     app_handle_stop_stream_request
 *
 * Purpose:
 *     Process a STOP_STREAM request.
 *
 * Input Parameters:
 *     request:
 *         Request frame.
 *
 * Output Parameters:
 *     None.
 *
 * Return Value:
 *     None.
 */
static void app_handle_stop_stream_request(const protocol_frame_t *request)
{
    protocol_command_result_t result_code;

    if (request->payload_length != 0u)
    {
        result_code = PROTOCOL_COMMAND_RESULT_INVALID_PAYLOAD;
    }
    else if (s_device_state == DEVICE_STATE_STREAMING)
    {
        app_transition_to(DEVICE_STATE_IDLE);
        result_code = PROTOCOL_COMMAND_RESULT_OK;
    }
    else
    {
        result_code = PROTOCOL_COMMAND_RESULT_INVALID_STATE;
    }

    app_send_control_response(PROTOCOL_MESSAGE_STOP_STREAM_RESPONSE,
                              request->sequence,
                            result_code);
}

/*
 * Function:
 *     app_handle_request
 *
 * Purpose:
 *     Dispatch one validated request frame.
 *
 * Input Parameters:
 *     request:
 *         Request frame.
 *
 * Output Parameters:
 *     None.
 *
 * Return Value:
 *     None.
 */
static void app_handle_request(const protocol_frame_t *request)
{
    if (request->flags != PROTOCOL_FLAG_REQUEST)
    {
        app_send_error_response(request->message_id,
                                request->sequence,
                              PROTOCOL_COMMAND_RESULT_UNSUPPORTED);
        return;
    }

    switch (request->message_id)
    {
        case PROTOCOL_MESSAGE_PING_REQUEST:
            app_handle_ping_request(request);
            break;

        case PROTOCOL_MESSAGE_GET_DEVICE_INFO_REQUEST:
            app_handle_device_info_request(request);
            break;

        case PROTOCOL_MESSAGE_START_STREAM_REQUEST:
            app_handle_start_stream_request(request);
            break;

        case PROTOCOL_MESSAGE_STOP_STREAM_REQUEST:
            app_handle_stop_stream_request(request);
            break;

        default:
            app_send_error_response(request->message_id,
                                    request->sequence,
                                  PROTOCOL_COMMAND_RESULT_UNSUPPORTED);
            break;
    }
}

/*
 * Function:
 *     app_process_received_bytes
 *
 * Purpose:
 *     Drain received UART bytes and feed the protocol parser.
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
    protocol_frame_t frame;
    protocol_parse_result_t parse_result;

    while (serial_transport_read_byte(&data_byte) == true)
    {
        parse_result = protocol_parser_push_byte(&s_protocol_parser, data_byte, &frame);

        if (parse_result == PROTOCOL_PARSE_FRAME_READY)
        {
            app_handle_request(&frame);
        }
    }
}

/*
 * Function:
 *     app_update_led
 *
 * Purpose:
 *     Update the LED heartbeat for one 5 ms tick.
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
static void app_update_led(void)
{
    if (s_device_state != DEVICE_STATE_STREAMING)
    {
        s_is_led_on = false;
        s_led_tick_count = 0u;
        platform_led_set(false);
        return;
    }

    if (s_led_tick_count >= (LED_TOGGLE_TICK_COUNT - 1u))
    {
        s_led_tick_count = 0u;
        s_is_led_on = (s_is_led_on == false);
        platform_led_set(s_is_led_on);
    }
    else
    {
        s_led_tick_count += 1u;
    }
}

/*
 * Function:
 *     app_send_telemetry
 *
 * Purpose:
 *     Build and queue one telemetry frame.
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
static void app_send_telemetry(void)
{
    uint8_t payload[TELEMETRY_PAYLOAD_LENGTH];
    uint8_t status_flags;
    int16_t sample;
    uint16_t sequence;
    serial_transport_statistics_t statistics;

    sample = sine_generator_get_next_sample();
    sequence = app_take_telemetry_sequence();
    serial_transport_get_statistics(&statistics);

    status_flags = TELEMETRY_STATUS_STREAMING;
    if (s_has_uart_error == true)
    {
        status_flags |= TELEMETRY_STATUS_UART_ERROR;
    }
    if (s_last_event_overflow_count != 0u)
    {
        status_flags |= TELEMETRY_STATUS_EVENT_OVERFLOW;
    }
    if (s_tx_reject_count != 0u)
    {
        status_flags |= TELEMETRY_STATUS_TX_REJECTED;
    }

    protocol_write_u32_le(&payload[TELEMETRY_UPTIME_OFFSET], s_uptime_ms);
    protocol_write_u16_le(&payload[TELEMETRY_SAMPLE_OFFSET], (uint16_t)sample);
    payload[TELEMETRY_STATE_OFFSET] = (uint8_t)s_device_state;
    payload[TELEMETRY_STATUS_OFFSET] = status_flags;
    protocol_write_u16_le(&payload[TELEMETRY_EVENT_OVERFLOW_OFFSET],
                          app_saturate_to_u16(s_last_event_overflow_count));
    protocol_write_u16_le(&payload[TELEMETRY_RX_OVERFLOW_OFFSET],
                          app_saturate_to_u16(statistics.rx_overflow_count));
    protocol_write_u16_le(&payload[TELEMETRY_TX_OVERFLOW_OFFSET],
                          app_saturate_to_u16(statistics.tx_overflow_count));

    if (app_send_frame(PROTOCOL_MESSAGE_TELEMETRY,
                       PROTOCOL_FLAG_TELEMETRY,
                       sequence,
                       payload,
                       TELEMETRY_PAYLOAD_LENGTH) == false)
    {
        app_increment_saturating_u32(&s_tx_reject_count);
    }
}

/*
 * Function:
 *     app_process_one_tick
 *
 * Purpose:
 *     Process one 5 ms application tick.
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
static void app_process_one_tick(void)
{
    app_add_uptime(PLATFORM_TICK_PERIOD_MS);
    app_update_led();

    if (s_device_state == DEVICE_STATE_STREAMING)
    {
        app_send_telemetry();
    }
}

/*
 * Function:
 *     app_init
 *
 * Purpose:
 *     Initialize application state and protocol processing.
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
    s_uptime_ms = 0u;
    s_telemetry_sequence = 0u;
    s_led_tick_count = 0u;
    s_is_led_on = false;
    s_has_uart_error = false;
    s_last_event_overflow_count = 0u;
    s_tx_reject_count = 0u;

    protocol_parser_init(&s_protocol_parser);
    app_transition_to(DEVICE_STATE_IDLE);
}

/*
 * Function:
 *     app_process_events
 *
 * Purpose:
 *     Processes one coherent event batch in the main execution context.
 *
 * Input Parameters:
 *     event_batch:
 *         Points to a caller-owned event batch. The function does not
 *         retain or modify the referenced object.
 *
 * Output Parameters:
 *     None.
 *
 * Return Value:
 *     None.
 *
 * Notes:
 *     A NULL pointer is ignored. The function shall not be called from
 *         interrupt context.
 */
void app_process_events(const app_event_batch_t *event_batch)
{
    uint16_t processed_tick_count;

    if (event_batch == NULL)
    {
        return;
    }

    s_last_event_overflow_count = event_batch->tick_overflow_count;

    if ((event_batch->flags & APP_EVENT_FLAG_UART_ERROR) != 0u)
    {
        s_has_uart_error = true;
    }

    if ((event_batch->flags & APP_EVENT_FLAG_UART_RX_AVAILABLE) != 0u)
    {
        app_process_received_bytes();
    }

    processed_tick_count = 0u;
    while (processed_tick_count < event_batch->tick_count)
    {
        app_process_one_tick();
        processed_tick_count += 1u;
    }
}
