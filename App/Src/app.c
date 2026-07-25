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
#define FW_VERSION_PATCH                    (4u)
#define BOARD_ID_NUCLEO_F446RE              (1u)
#define TRANSPORT_ID_USART2_STLINK_VCP      (1u)
#define SAMPLE_PERIOD_US                    (5000u)
#define DEVICE_CAPABILITY_STREAMING         (1u << 0u)
#define DEVICE_CAPABILITY_CRC16             (1u << 1u)
#define DEVICE_CAPABILITY_EVENT_DRIVEN      (1u << 2u)

#define PING_RESPONSE_LENGTH                (6u)
#define DEVICE_INFO_RESPONSE_LENGTH         (12u)
#define ERROR_RESPONSE_LENGTH               (4u)
#define CONTROL_RESPONSE_LENGTH             (2u)
#define TELEMETRY_PAYLOAD_LENGTH            (14u)
#define LED_TOGGLE_TICK_COUNT               (100u)

#define TELEMETRY_STATUS_STREAMING          (1u << 0u)
#define TELEMETRY_STATUS_UART_ERROR         (1u << 1u)
#define TELEMETRY_STATUS_EVENT_OVERFLOW     (1u << 2u)

static device_state_t s_device_state;
static protocol_parser_t s_protocol_parser;
static uint32_t s_uptime_ms;
static uint16_t s_telemetry_sequence;
static uint16_t s_led_tick_count;
static bool s_led_is_on;
static bool s_uart_error_seen;
static uint32_t s_last_event_overflow_count;

/**
 * @brief Add elapsed milliseconds without wrapping uptime.
 * @param elapsed_ms Elapsed time.
 */
static void App_AddUptime(uint32_t elapsed_ms)
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

/**
 * @brief Return a 16-bit saturation of a 32-bit diagnostic counter.
 * @param value Source value.
 * @return Saturated value.
 */
static uint16_t App_SaturateToU16(uint32_t value)
{
    if (value > UINT16_MAX)
    {
        return UINT16_MAX;
    }

    return (uint16_t)value;
}

/**
 * @brief Advance the protocol telemetry sequence modulo 65536.
 *
 * Intentional wraparound is isolated here because the wire-format sequence is
 * defined as a modulo-65536 counter. The returned value identifies the sample
 * attempt, including attempts dropped because the TX queue had no capacity.
 *
 * @return Current sequence before advancing.
 */
static uint16_t App_TakeTelemetrySequence(void)
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

/**
 * @brief Encode and queue one protocol frame.
 * @param message_id Message identifier.
 * @param flags Frame flags.
 * @param sequence Sequence value.
 * @param payload Payload pointer.
 * @param payload_length Payload length.
 * @return True when queued for transmission.
 */
static bool App_SendFrame(uint16_t message_id,
                          uint8_t flags,
                          uint16_t sequence,
                          const uint8_t *payload,
                          uint8_t payload_length)
{
    uint8_t encoded_frame[PROTOCOL_MAX_FRAME_LENGTH];
    size_t encoded_length;
    bool encoded;
    serial_transport_result_t transport_result;

    encoded = Protocol_EncodeFrame(message_id,
                                   flags,
                                   sequence,
                                   payload,
                                   payload_length,
                                   encoded_frame,
                                   sizeof(encoded_frame),
                                   &encoded_length);

    if (encoded == false)
    {
        return false;
    }

    transport_result = SerialTransport_Write(encoded_frame, encoded_length);
    return (transport_result == SERIAL_TRANSPORT_RESULT_OK);
}

/**
 * @brief Send a generic error response for a rejected request.
 * @param request_message_id Rejected request message identifier.
 * @param request_sequence Request sequence to echo.
 * @param result_code Rejection result code.
 */
static void App_SendErrorResponse(uint16_t request_message_id,
                                  uint16_t request_sequence,
                                  protocol_command_result_t result_code)
{
    uint8_t payload[ERROR_RESPONSE_LENGTH];

    Protocol_WriteU16Le(&payload[0], request_message_id);
    payload[2] = (uint8_t)result_code;
    payload[3] = (uint8_t)s_device_state;

    (void)App_SendFrame(PROTOCOL_MESSAGE_ERROR_RESPONSE,
                        PROTOCOL_FLAG_RESPONSE,
                        request_sequence,
                        payload,
                        ERROR_RESPONSE_LENGTH);
}

/**
 * @brief Send a dedicated start/stop control response.
 * @param response_message_id Dedicated response identifier.
 * @param request_sequence Request sequence to echo.
 * @param result_code Command result code.
 */
static void App_SendControlResponse(uint16_t response_message_id,
                                    uint16_t request_sequence,
                                    protocol_command_result_t result_code)
{
    uint8_t payload[CONTROL_RESPONSE_LENGTH];

    payload[0] = (uint8_t)result_code;
    payload[1] = (uint8_t)s_device_state;

    (void)App_SendFrame(response_message_id,
                        PROTOCOL_FLAG_RESPONSE,
                        request_sequence,
                        payload,
                        CONTROL_RESPONSE_LENGTH);
}

/**
 * @brief Process a validated PING request.
 * @param request Request frame.
 */
static void App_HandlePingRequest(const protocol_frame_t *request)
{
    uint8_t payload[PING_RESPONSE_LENGTH];

    if (request->payload_length != 0u)
    {
        App_SendErrorResponse(request->message_id,
                              request->sequence,
                              PROTOCOL_COMMAND_RESULT_INVALID_PAYLOAD);
        return;
    }

    Protocol_WriteU32Le(&payload[0], s_uptime_ms);
    payload[4] = (uint8_t)s_device_state;
    payload[5] = PROTOCOL_VERSION;

    (void)App_SendFrame(PROTOCOL_MESSAGE_PING_RESPONSE,
                        PROTOCOL_FLAG_RESPONSE,
                        request->sequence,
                        payload,
                        PING_RESPONSE_LENGTH);
}

/**
 * @brief Process a validated GET_DEVICE_INFO request.
 * @param request Request frame.
 */
static void App_HandleDeviceInfoRequest(const protocol_frame_t *request)
{
    uint8_t payload[DEVICE_INFO_RESPONSE_LENGTH];
    const uint8_t capabilities = (uint8_t)(DEVICE_CAPABILITY_STREAMING |
                                            DEVICE_CAPABILITY_CRC16 |
                                            DEVICE_CAPABILITY_EVENT_DRIVEN);

    if (request->payload_length != 0u)
    {
        App_SendErrorResponse(request->message_id,
                              request->sequence,
                              PROTOCOL_COMMAND_RESULT_INVALID_PAYLOAD);
        return;
    }

    payload[0] = PROTOCOL_VERSION;
    payload[1] = FW_VERSION_MAJOR;
    payload[2] = FW_VERSION_MINOR;
    payload[3] = FW_VERSION_PATCH;
    payload[4] = BOARD_ID_NUCLEO_F446RE;
    payload[5] = TRANSPORT_ID_USART2_STLINK_VCP;
    Protocol_WriteU16Le(&payload[6], SAMPLE_PERIOD_US);
    payload[8] = PROTOCOL_MAX_PAYLOAD_LENGTH;
    payload[9] = capabilities;
    payload[10] = 0u;
    payload[11] = 0u;

    (void)App_SendFrame(PROTOCOL_MESSAGE_DEVICE_INFO_RESPONSE,
                        PROTOCOL_FLAG_RESPONSE,
                        request->sequence,
                        payload,
                        DEVICE_INFO_RESPONSE_LENGTH);
}

/**
 * @brief Process a START_STREAM request.
 * @param request Request frame.
 */
static void App_HandleStartStreamRequest(const protocol_frame_t *request)
{
    protocol_command_result_t result_code;

    if (request->payload_length != 0u)
    {
        result_code = PROTOCOL_COMMAND_RESULT_INVALID_PAYLOAD;
    }
    else if (s_device_state == DEVICE_STATE_IDLE)
    {
        s_device_state = DEVICE_STATE_STREAMING;
        s_telemetry_sequence = 0u;
        s_led_tick_count = 0u;
        SineGenerator_Reset();
        result_code = PROTOCOL_COMMAND_RESULT_OK;
    }
    else
    {
        result_code = PROTOCOL_COMMAND_RESULT_INVALID_STATE;
    }

    App_SendControlResponse(PROTOCOL_MESSAGE_START_STREAM_RESPONSE,
                            request->sequence,
                            result_code);
}

/**
 * @brief Process a STOP_STREAM request.
 * @param request Request frame.
 */
static void App_HandleStopStreamRequest(const protocol_frame_t *request)
{
    protocol_command_result_t result_code;

    if (request->payload_length != 0u)
    {
        result_code = PROTOCOL_COMMAND_RESULT_INVALID_PAYLOAD;
    }
    else if (s_device_state == DEVICE_STATE_STREAMING)
    {
        s_device_state = DEVICE_STATE_IDLE;
        s_led_is_on = false;
        s_led_tick_count = 0u;
        Platform_LedSet(false);
        result_code = PROTOCOL_COMMAND_RESULT_OK;
    }
    else
    {
        result_code = PROTOCOL_COMMAND_RESULT_INVALID_STATE;
    }

    App_SendControlResponse(PROTOCOL_MESSAGE_STOP_STREAM_RESPONSE,
                            request->sequence,
                            result_code);
}

/**
 * @brief Dispatch one validated request frame.
 * @param request Request frame.
 */
static void App_HandleRequest(const protocol_frame_t *request)
{
    if (request->flags != PROTOCOL_FLAG_REQUEST)
    {
        App_SendErrorResponse(request->message_id,
                              request->sequence,
                              PROTOCOL_COMMAND_RESULT_UNSUPPORTED);
        return;
    }

    switch (request->message_id)
    {
        case PROTOCOL_MESSAGE_PING_REQUEST:
            App_HandlePingRequest(request);
            break;

        case PROTOCOL_MESSAGE_GET_DEVICE_INFO_REQUEST:
            App_HandleDeviceInfoRequest(request);
            break;

        case PROTOCOL_MESSAGE_START_STREAM_REQUEST:
            App_HandleStartStreamRequest(request);
            break;

        case PROTOCOL_MESSAGE_STOP_STREAM_REQUEST:
            App_HandleStopStreamRequest(request);
            break;

        default:
            App_SendErrorResponse(request->message_id,
                                  request->sequence,
                                  PROTOCOL_COMMAND_RESULT_UNSUPPORTED);
            break;
    }
}

/**
 * @brief Drain received UART bytes and feed the protocol parser.
 */
static void App_ProcessReceivedBytes(void)
{
    uint8_t data_byte;
    protocol_frame_t frame;
    protocol_parse_result_t parse_result;

    while (SerialTransport_ReadByte(&data_byte) == true)
    {
        parse_result = ProtocolParser_PushByte(&s_protocol_parser, data_byte, &frame);

        if (parse_result == PROTOCOL_PARSE_FRAME_READY)
        {
            App_HandleRequest(&frame);
        }
    }
}

/**
 * @brief Update the LED heartbeat for one 5 ms tick.
 */
static void App_UpdateLed(void)
{
    if (s_device_state != DEVICE_STATE_STREAMING)
    {
        s_led_is_on = false;
        s_led_tick_count = 0u;
        Platform_LedSet(false);
        return;
    }

    if (s_led_tick_count >= (LED_TOGGLE_TICK_COUNT - 1u))
    {
        s_led_tick_count = 0u;
        s_led_is_on = (s_led_is_on == false);
        Platform_LedSet(s_led_is_on);
    }
    else
    {
        s_led_tick_count += 1u;
    }
}

/**
 * @brief Build and queue one telemetry frame.
 */
static void App_SendTelemetry(void)
{
    uint8_t payload[TELEMETRY_PAYLOAD_LENGTH];
    uint8_t status_flags;
    int16_t sample;
    uint16_t sequence;
    serial_transport_statistics_t statistics;

    sample = SineGenerator_GetNextSample();
    sequence = App_TakeTelemetrySequence();
    SerialTransport_GetStatistics(&statistics);

    status_flags = TELEMETRY_STATUS_STREAMING;
    if (s_uart_error_seen == true)
    {
        status_flags |= TELEMETRY_STATUS_UART_ERROR;
    }
    if (s_last_event_overflow_count != 0u)
    {
        status_flags |= TELEMETRY_STATUS_EVENT_OVERFLOW;
    }

    Protocol_WriteU32Le(&payload[0], s_uptime_ms);
    Protocol_WriteU16Le(&payload[4], (uint16_t)sample);
    payload[6] = (uint8_t)s_device_state;
    payload[7] = status_flags;
    Protocol_WriteU16Le(&payload[8], App_SaturateToU16(s_last_event_overflow_count));
    Protocol_WriteU16Le(&payload[10], App_SaturateToU16(statistics.rx_overflow_count));
    Protocol_WriteU16Le(&payload[12], App_SaturateToU16(statistics.tx_overflow_count));

    (void)App_SendFrame(PROTOCOL_MESSAGE_TELEMETRY,
                        PROTOCOL_FLAG_TELEMETRY,
                        sequence,
                        payload,
                        TELEMETRY_PAYLOAD_LENGTH);
}

/**
 * @brief Process one 5 ms application tick.
 */
static void App_ProcessOneTick(void)
{
    App_AddUptime(PLATFORM_TICK_PERIOD_MS);
    App_UpdateLed();

    if (s_device_state == DEVICE_STATE_STREAMING)
    {
        App_SendTelemetry();
    }
}

/**
 * @brief Initialize application state and protocol processing.
 */
void App_Init(void)
{
    s_device_state = DEVICE_STATE_IDLE;
    s_uptime_ms = 0u;
    s_telemetry_sequence = 0u;
    s_led_tick_count = 0u;
    s_led_is_on = false;
    s_uart_error_seen = false;
    s_last_event_overflow_count = 0u;

    ProtocolParser_Init(&s_protocol_parser);
    SineGenerator_Reset();
    Platform_LedSet(false);
}

/**
 * @brief Process one batch of events in main context.
 * @param event_batch Event batch.
 */
void App_ProcessEvents(const app_event_batch_t *event_batch)
{
    uint16_t processed_tick_count;

    if (event_batch == NULL)
    {
        return;
    }

    s_last_event_overflow_count = event_batch->tick_overflow_count;

    if ((event_batch->flags & APP_EVENT_FLAG_UART_ERROR) != 0u)
    {
        s_uart_error_seen = true;
    }

    if ((event_batch->flags & APP_EVENT_FLAG_UART_RX_AVAILABLE) != 0u)
    {
        App_ProcessReceivedBytes();
    }

    processed_tick_count = 0u;
    while (processed_tick_count < event_batch->tick_count)
    {
        App_ProcessOneTick();
        processed_tick_count += 1u;
    }
}
