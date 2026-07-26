// Copyright (c) 2026 Ray Yang. All rights reserved.
//
// File:
//     app.c
//
// Purpose:
//     Implements the firmware application state machine and command processing.
//
// Responsibilities:
//     - Validates and processes Host protocol commands.
//     - Owns streaming state, timing, sequence numbers, and waveform rotation.
//     - Encodes bounded responses and telemetry through the transport adapter.
//
// Notes:
//     All application behavior executes in main context through ordered events.

#include "app.h"

#include <stdbool.h>
#include <limits.h>
#include <stddef.h>
#include <stdint.h>

#include "device_state.h"
#include "platform.h"
#include "protocol.h"
#include "protocol_messages.h"
#include "serial_transport.h"
#include "waveform_generator.h"

#define FW_VERSION_MAJOR                       (0u)
#define FW_VERSION_MINOR                       (2u)
#define FW_VERSION_PATCH                       (8u)
#define MICROSECONDS_PER_SECOND                (1000000u)
#define WAVEFORM_SWITCH_INTERVAL_US             (10000000u)
#define ACK_NACK_PAYLOAD_LENGTH                 (3u)
#define ACK_NACK_REQUEST_ID_OFFSET               (0u)
#define ACK_NACK_RESULT_OFFSET                   (1u)
#define ACK_NACK_STATE_OFFSET                    (2u)
#define DEVICE_INFO_FIXED_PAYLOAD_LENGTH         (8u)
#define DEVICE_INFO_DEVICE_TYPE_OFFSET           (0u)
#define DEVICE_INFO_VERSION_MAJOR_OFFSET         (2u)
#define DEVICE_INFO_VERSION_MINOR_OFFSET         (3u)
#define DEVICE_INFO_VERSION_PATCH_OFFSET         (4u)
#define DEVICE_INFO_MAX_STREAM_RATE_OFFSET       (5u)
#define DEVICE_INFO_NAME_LENGTH_OFFSET           (7u)
#define DEVICE_INFO_NAME_OFFSET                  (8u)
#define STREAM_CONFIG_PAYLOAD_LENGTH             (2u)
#define TELEMETRY_PAYLOAD_LENGTH                 (14u)
#define TELEMETRY_SAMPLE_COUNTER_OFFSET          (0u)
#define TELEMETRY_DEVICE_TICK_OFFSET             (4u)
#define TELEMETRY_SAMPLE_VALUE_OFFSET            (8u)
#define TELEMETRY_STATUS_FLAGS_OFFSET            (12u)
#define APP_SEQUENCE_FIRST                       (1u)
#define APP_LED_TOGGLE_SAMPLE_MASK               (0x7Fu)
#define TELEMETRY_FRAME_LENGTH                 \
    (PROTOCOL_FRAME_OVERHEAD_LENGTH + TELEMETRY_PAYLOAD_LENGTH)
#define APP_STREAMING_SUPPORTED                \
    ((SERIAL_BAUD_IS_COMMAND_ONLY(SERIAL_TRANSPORT_BAUD_RATE) == 0u) \
         ? 1u                                                          \
         : 0u)
#define APP_CALCULATED_STREAM_MIN_INTERVAL_US  \
    SERIAL_BAUD_CALCULATED_MIN_INTERVAL_US(    \
        SERIAL_BAUD_PERIPHERAL_CLOCK_HZ,       \
        SERIAL_TRANSPORT_BAUD_RATE,            \
        TELEMETRY_FRAME_LENGTH,                \
        SERIAL_TRANSPORT_BITS_PER_BYTE,        \
        SERIAL_BAUD_STREAM_RESERVE_PERCENT)
#define APP_EFFECTIVE_STREAM_MIN_INTERVAL_US   \
    ((APP_CALCULATED_STREAM_MIN_INTERVAL_US > PROTOCOL_STREAM_INTERVAL_MIN_US) \
         ? APP_CALCULATED_STREAM_MIN_INTERVAL_US                              \
         : PROTOCOL_STREAM_INTERVAL_MIN_US)
#define APP_INITIAL_STREAM_INTERVAL_US         \
    (((APP_STREAMING_SUPPORTED != 0u)                                        \
      && (PROTOCOL_STREAM_INTERVAL_DEFAULT_US                                \
          < APP_EFFECTIVE_STREAM_MIN_INTERVAL_US))                           \
         ? APP_EFFECTIVE_STREAM_MIN_INTERVAL_US                              \
         : PROTOCOL_STREAM_INTERVAL_DEFAULT_US)
#define APP_MAX_STREAM_RATE_HZ                 \
    ((APP_STREAMING_SUPPORTED != 0u)                                        \
         ? (MICROSECONDS_PER_SECOND / APP_EFFECTIVE_STREAM_MIN_INTERVAL_US) \
         : 0u)

_Static_assert(SERIAL_BAUD_IS_SUPPORTED(SERIAL_TRANSPORT_BAUD_RATE) != 0u,
               "Unsupported configured UART baud rate.");
_Static_assert((APP_STREAMING_SUPPORTED == 0u)
                   || (APP_EFFECTIVE_STREAM_MIN_INTERVAL_US
                       <= PROTOCOL_STREAM_INTERVAL_MAX_US),
               "Configured UART baud cannot support the protocol stream range.");

static device_state_t s_state;
static protocol_parser_t s_parser;
static protocol_frame_t s_frame;
static uint8_t s_encoded[PROTOCOL_MAX_FRAME_LENGTH];
static uint16_t s_interval_us;
static uint16_t s_unsolicited_sequence;
static uint32_t s_tick_us;
static uint32_t s_waveform_phase_us;
static uint32_t s_waveform_elapsed_us;
static uint32_t s_sample_counter;
static waveform_type_t s_waveform;
static bool s_has_uart_error;

static const uint8_t s_name[] =
{
    'N', 'U', 'C', 'L', 'E', 'O', '-', 'F', '4', '4', '6', 'R', 'E'
};

/*
 * Function:
 *     app_send_frame
 *
 * Purpose:
 *     Encodes one protocol frame and submits it to the bounded serial transport.
 *
 * Input Parameters:
 *     message_id:
 *         Protocol message identifier to encode.
 *     sequence:
 *         Protocol sequence number to encode.
 *     payload:
 *         Pointer to payload bytes, or NULL when payload_length is zero.
 *     payload_length:
 *         Number of valid payload bytes.
 *
 * Output Parameters:
 *     None.
 *
 * Return Value:
 *     true:
 *         The frame was encoded and accepted by the transport.
 *     false:
 *         Encoding failed or the transport could not accept the frame.
 */
static bool app_send_frame(uint8_t message_id,
                           uint16_t sequence,
                           const uint8_t *payload,
                           uint16_t payload_length)
{
    size_t encoded_length;
    bool is_encoded;

    is_encoded = protocol_encode_frame(message_id,
                                    sequence,
                                    payload,
                                    payload_length,
                                    s_encoded,
                                    sizeof(s_encoded),
                                    &encoded_length);
    if (is_encoded == false)
    {
        return false;
    }

    return serial_transport_write(s_encoded, encoded_length)
        == SERIAL_TRANSPORT_RESULT_OK;
}

/*
 * Function:
 *     app_send_ack
 *
 * Purpose:
 *     Builds and sends an ACK response for a valid Host request.
 *
 * Input Parameters:
 *     request_id:
 *         Message identifier of the acknowledged request.
 *     sequence:
 *         Sequence number copied from the request.
 *
 * Output Parameters:
 *     None.
 *
 * Return Value:
 *     None.
 */
static void app_send_ack(uint8_t request_id, uint16_t sequence)
{
    const uint8_t payload[ACK_NACK_PAYLOAD_LENGTH] =
    {
        [ACK_NACK_REQUEST_ID_OFFSET] = request_id,
        [ACK_NACK_RESULT_OFFSET] = (uint8_t)PROTOCOL_RESULT_OK,
        [ACK_NACK_STATE_OFFSET] = (uint8_t)s_state
    };

    (void)app_send_frame(PROTOCOL_MESSAGE_ACK,
                         sequence,
                         payload,
                         (uint16_t)sizeof(payload));
}

/*
 * Function:
 *     app_send_nack
 *
 * Purpose:
 *     Builds and sends a NACK response for a rejected Host request.
 *
 * Input Parameters:
 *     request_id:
 *         Message identifier of the rejected request.
 *     sequence:
 *         Sequence number copied from the request.
 *     result:
 *         Protocol result code explaining the rejection.
 *
 * Output Parameters:
 *     None.
 *
 * Return Value:
 *     None.
 */
static void app_send_nack(uint8_t request_id,
                          uint16_t sequence,
                          protocol_result_code_t result)
{
    const uint8_t payload[ACK_NACK_PAYLOAD_LENGTH] =
    {
        [ACK_NACK_REQUEST_ID_OFFSET] = request_id,
        [ACK_NACK_RESULT_OFFSET] = (uint8_t)result,
        [ACK_NACK_STATE_OFFSET] = (uint8_t)s_state
    };

    (void)app_send_frame(PROTOCOL_MESSAGE_NACK,
                         sequence,
                         payload,
                         (uint16_t)sizeof(payload));
}

/*
 * Function:
 *     app_send_device_info
 *
 * Purpose:
 *     Builds and sends the current device-information response.
 *
 * Input Parameters:
 *     sequence:
 *         Sequence number copied from the request.
 *
 * Output Parameters:
 *     None.
 *
 * Return Value:
 *     None.
 */
static void app_send_device_info(uint16_t sequence)
{
    uint8_t payload[DEVICE_INFO_FIXED_PAYLOAD_LENGTH + sizeof(s_name)];
    size_t name_index;

    protocol_write_u16_le(&payload[DEVICE_INFO_DEVICE_TYPE_OFFSET],
                          PROTOCOL_DEVICE_TYPE_STM32F446RE);
    payload[DEVICE_INFO_VERSION_MAJOR_OFFSET] = FW_VERSION_MAJOR;
    payload[DEVICE_INFO_VERSION_MINOR_OFFSET] = FW_VERSION_MINOR;
    payload[DEVICE_INFO_VERSION_PATCH_OFFSET] = FW_VERSION_PATCH;
    protocol_write_u16_le(&payload[DEVICE_INFO_MAX_STREAM_RATE_OFFSET],
                          (uint16_t)APP_MAX_STREAM_RATE_HZ);
    payload[DEVICE_INFO_NAME_LENGTH_OFFSET] = (uint8_t)sizeof(s_name);

    for (name_index = 0u; name_index < sizeof(s_name); ++name_index)
    {
        payload[DEVICE_INFO_NAME_OFFSET + name_index] = s_name[name_index];
    }

    (void)app_send_frame(PROTOCOL_MESSAGE_DEVICE_INFO,
                         sequence,
                         payload,
                         (uint16_t)sizeof(payload));
}

/*
 * Function:
 *     app_process_command
 *
 * Purpose:
 *     Validates one decoded command and performs the permitted state transition
 *     or configuration update.
 *
 * Input Parameters:
 *     frame:
 *         Pointer to the decoded command frame. The frame is not modified.
 *
 * Output Parameters:
 *     None.
 *
 * Return Value:
 *     None.
 *
 * Notes:
 *     Runs only in main context and is the owner of device-state changes.
 */
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
            if ((s_state != DEVICE_STATE_IDLE)
                || (APP_STREAMING_SUPPORTED == 0u))
            {
                app_send_nack(frame->message_id,
                              frame->sequence,
                              PROTOCOL_RESULT_INVALID_STATE);
                break;
            }
            if (frame->payload_length != STREAM_CONFIG_PAYLOAD_LENGTH)
            {
                app_send_nack(frame->message_id,
                              frame->sequence,
                              PROTOCOL_RESULT_INVALID_LENGTH);
                break;
            }

            interval_us = protocol_read_u16_le(frame->payload);
            if ((interval_us < APP_EFFECTIVE_STREAM_MIN_INTERVAL_US)
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
            else if ((s_state != DEVICE_STATE_IDLE)
                     || (APP_STREAMING_SUPPORTED == 0u))
            {
                app_send_nack(frame->message_id,
                              frame->sequence,
                              PROTOCOL_RESULT_INVALID_STATE);
            }
            else
            {
                s_state = DEVICE_STATE_STREAMING;
                s_unsolicited_sequence = APP_SEQUENCE_FIRST;
                s_sample_counter = 0u;
                s_waveform = WAVEFORM_TYPE_SINE;
                s_waveform_phase_us = 0u;
                s_waveform_elapsed_us = 0u;
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

/*
 * Function:
 *     app_process_rx_byte
 *
 * Purpose:
 *     Pushes one received byte into the protocol parser and dispatches a complete
 *     command frame when available.
 *
 * Input Parameters:
 *     data_byte:
 *         One byte received from USART2.
 *
 * Output Parameters:
 *     None.
 *
 * Return Value:
 *     None.
 */
static void app_process_rx_byte(uint8_t data_byte)
{
    const protocol_parse_result_t result =
        protocol_parser_push_byte(&s_parser, data_byte, &s_frame);

    if (result == PROTOCOL_PARSE_FRAME_READY)
    {
        app_process_command(&s_frame);
    }
}

/*
 * Function:
 *     app_process_tick
 *
 * Purpose:
 *     Advances application time and emits one telemetry frame when streaming.
 *
 * Input Parameters:
 *     None.
 *
 * Output Parameters:
 *     None.
 *
 * Return Value:
 *     None.
 *
 * Notes:
 *     Runs in main context after an ordered sample-tick event is dequeued.
 */
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
    s_waveform_elapsed_us += s_interval_us;
    s_waveform_phase_us += s_interval_us;

    if (s_waveform_elapsed_us >= WAVEFORM_SWITCH_INTERVAL_US)
    {
        s_waveform_elapsed_us -= WAVEFORM_SWITCH_INTERVAL_US;
        s_waveform = waveform_generator_next(s_waveform);
        s_waveform_phase_us = s_waveform_elapsed_us;
    }

    while (s_waveform_phase_us >= waveform_generator_period_us(s_waveform))
    {
        s_waveform_phase_us -= waveform_generator_period_us(s_waveform);
    }

    if (serial_transport_get_rx_overflow_count() != 0u)
    {
        status_flags |= PROTOCOL_STATUS_RX_OVERFLOW_OBSERVED;
    }
    if (serial_transport_get_tx_overflow_count() != 0u)
    {
        status_flags |= PROTOCOL_STATUS_TX_OVERFLOW_OBSERVED;
    }
    if ((s_has_uart_error == true) || (serial_transport_get_uart_error_count() != 0u))
    {
        status_flags |= PROTOCOL_STATUS_UART_ERROR_OBSERVED;
    }

    protocol_write_u32_le(&payload[TELEMETRY_SAMPLE_COUNTER_OFFSET],
                          s_sample_counter);
    protocol_write_u32_le(&payload[TELEMETRY_DEVICE_TICK_OFFSET], s_tick_us);
    protocol_write_float32_le(
        &payload[TELEMETRY_SAMPLE_VALUE_OFFSET],
        waveform_generator_sample(s_waveform, s_waveform_phase_us));
    protocol_write_u16_le(&payload[TELEMETRY_STATUS_FLAGS_OFFSET],
                          status_flags);

    (void)app_send_frame(PROTOCOL_MESSAGE_TELEMETRY_SAMPLE,
                         s_unsolicited_sequence,
                         payload,
                         TELEMETRY_PAYLOAD_LENGTH);
    if (s_unsolicited_sequence == UINT16_MAX)
    {
        s_unsolicited_sequence = APP_SEQUENCE_FIRST;
    }
    else
    {
        s_unsolicited_sequence = (uint16_t)(s_unsolicited_sequence + 1u);
    }
    platform_led_set((s_sample_counter & APP_LED_TOGGLE_SAMPLE_MASK) == 0u);
}

/*
 * Function:
 *     app_init
 *
 * Purpose:
 *     Initializes application-owned state, parser state, and the initial
 *     sample-timer interval.
 *
 * Input Parameters:
 *     None.
 *
 * Output Parameters:
 *     None.
 *
 * Return Value:
 *     None.
 *
 * Notes:
 *     Call once from main context after platform and transport initialization.
 */
void app_init(void)
{
    s_state = DEVICE_STATE_IDLE;
    s_interval_us = (uint16_t)APP_INITIAL_STREAM_INTERVAL_US;
    s_unsolicited_sequence = APP_SEQUENCE_FIRST;
    s_tick_us = 0u;
    s_waveform = WAVEFORM_TYPE_SINE;
    s_waveform_phase_us = 0u;
    s_waveform_elapsed_us = 0u;
    s_sample_counter = 0u;
    s_has_uart_error = false;
    protocol_parser_init(&s_parser);
    platform_sample_timer_set_interval_us(s_interval_us);
}

/*
 * Function:
 *     app_process_event
 *
 * Purpose:
 *     Validates and dispatches one ordered application event.
 *
 * Input Parameters:
 *     event:
 *         Pointer to the event to process. The referenced event is not modified.
 *
 * Output Parameters:
 *     None.
 *
 * Return Value:
 *     None.
 *
 * Notes:
 *     Runs in main context and owns all application state transitions.
 */
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
            s_has_uart_error = true;
            break;

        default:
            break;
    }
}
