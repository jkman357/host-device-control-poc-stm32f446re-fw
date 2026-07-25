#include "protocol.h"

#include <limits.h>

#include "protocol_crc.h"

#define HEADER_INDEX_VERSION         (0u)
#define HEADER_INDEX_MESSAGE_ID_LOW  (1u)
#define HEADER_INDEX_MESSAGE_ID_HIGH (2u)
#define HEADER_INDEX_FLAGS           (3u)
#define HEADER_INDEX_PAYLOAD_LENGTH  (4u)
#define HEADER_INDEX_SEQUENCE_LOW    (5u)
#define HEADER_INDEX_SEQUENCE_HIGH   (6u)

/**
 * @brief Saturating increment for parser diagnostic counters.
 * @param counter Counter to increment.
 */
static void Protocol_IncrementSaturatingU32(uint32_t *counter)
{
    if (*counter < UINT32_MAX)
    {
        *counter += 1u;
    }
}

/**
 * @brief Reset parser working state while preserving diagnostics.
 * @param parser Parser instance.
 */
static void ProtocolParser_ResetWorkingState(protocol_parser_t *parser)
{
    parser->state = PROTOCOL_PARSER_WAIT_SOF_0;
    parser->header_index = 0u;
    parser->payload_index = 0u;
    parser->calculated_crc = PROTOCOL_CRC_INITIAL_VALUE;
    parser->received_crc = 0u;
}

/**
 * @brief Copy decoded header fields into a frame.
 * @param parser Parser instance.
 * @param frame Destination frame.
 */
static void ProtocolParser_CopyHeader(const protocol_parser_t *parser,
                                      protocol_frame_t *frame)
{
    frame->message_id = (uint16_t)parser->header[HEADER_INDEX_MESSAGE_ID_LOW] |
                        (uint16_t)((uint16_t)parser->header[HEADER_INDEX_MESSAGE_ID_HIGH] << 8u);
    frame->flags = parser->header[HEADER_INDEX_FLAGS];
    frame->payload_length = parser->header[HEADER_INDEX_PAYLOAD_LENGTH];
    frame->sequence = (uint16_t)parser->header[HEADER_INDEX_SEQUENCE_LOW] |
                      (uint16_t)((uint16_t)parser->header[HEADER_INDEX_SEQUENCE_HIGH] << 8u);
}

/**
 * @brief Initialize a protocol parser instance.
 * @param parser Parser instance.
 */
void ProtocolParser_Init(protocol_parser_t *parser)
{
    if (parser != NULL)
    {
        parser->format_error_count = 0u;
        parser->crc_error_count = 0u;
        ProtocolParser_ResetWorkingState(parser);
    }
}

/**
 * @brief Push one received byte into the frame parser.
 * @param parser Parser instance.
 * @param data_byte Next received byte.
 * @param[out] frame Completed frame when the return value is FRAME_READY.
 * @return Parser result.
 */
protocol_parse_result_t ProtocolParser_PushByte(protocol_parser_t *parser,
                                                uint8_t data_byte,
                                                protocol_frame_t *frame)
{
    protocol_parse_result_t result;

    if ((parser == NULL) || (frame == NULL))
    {
        return PROTOCOL_PARSE_FORMAT_ERROR;
    }

    result = PROTOCOL_PARSE_NO_FRAME;

    switch (parser->state)
    {
        case PROTOCOL_PARSER_WAIT_SOF_0:
            if (data_byte == PROTOCOL_SOF_0)
            {
                parser->state = PROTOCOL_PARSER_WAIT_SOF_1;
            }
            break;

        case PROTOCOL_PARSER_WAIT_SOF_1:
            if (data_byte == PROTOCOL_SOF_1)
            {
                parser->state = PROTOCOL_PARSER_READ_HEADER;
                parser->header_index = 0u;
                parser->calculated_crc = PROTOCOL_CRC_INITIAL_VALUE;
            }
            else if (data_byte != PROTOCOL_SOF_0)
            {
                parser->state = PROTOCOL_PARSER_WAIT_SOF_0;
            }
            else
            {
                /* Keep waiting for SOF_1 after a repeated SOF_0. */
            }
            break;

        case PROTOCOL_PARSER_READ_HEADER:
            parser->header[parser->header_index] = data_byte;
            parser->calculated_crc = ProtocolCrc_Update(parser->calculated_crc, data_byte);

            if (parser->header_index < (PROTOCOL_FIXED_HEADER_LENGTH - 1u))
            {
                parser->header_index += 1u;
            }
            else
            {
                if ((parser->header[HEADER_INDEX_VERSION] != PROTOCOL_VERSION) ||
                    (parser->header[HEADER_INDEX_PAYLOAD_LENGTH] > PROTOCOL_MAX_PAYLOAD_LENGTH))
                {
                    Protocol_IncrementSaturatingU32(&parser->format_error_count);
                    ProtocolParser_ResetWorkingState(parser);
                    result = PROTOCOL_PARSE_FORMAT_ERROR;
                }
                else
                {
                    ProtocolParser_CopyHeader(parser, frame);
                    parser->payload_index = 0u;

                    if (frame->payload_length == 0u)
                    {
                        parser->state = PROTOCOL_PARSER_READ_CRC_LOW;
                    }
                    else
                    {
                        parser->state = PROTOCOL_PARSER_READ_PAYLOAD;
                    }
                }
            }
            break;

        case PROTOCOL_PARSER_READ_PAYLOAD:
            frame->payload[parser->payload_index] = data_byte;
            parser->calculated_crc = ProtocolCrc_Update(parser->calculated_crc, data_byte);

            if (parser->payload_index < (uint8_t)(frame->payload_length - 1u))
            {
                parser->payload_index += 1u;
            }
            else
            {
                parser->state = PROTOCOL_PARSER_READ_CRC_LOW;
            }
            break;

        case PROTOCOL_PARSER_READ_CRC_LOW:
            parser->received_crc = (uint16_t)data_byte;
            parser->state = PROTOCOL_PARSER_READ_CRC_HIGH;
            break;

        case PROTOCOL_PARSER_READ_CRC_HIGH:
            parser->received_crc |= (uint16_t)((uint16_t)data_byte << 8u);

            if (parser->received_crc == parser->calculated_crc)
            {
                result = PROTOCOL_PARSE_FRAME_READY;
            }
            else
            {
                Protocol_IncrementSaturatingU32(&parser->crc_error_count);
                result = PROTOCOL_PARSE_CRC_ERROR;
            }

            ProtocolParser_ResetWorkingState(parser);
            break;

        default:
            Protocol_IncrementSaturatingU32(&parser->format_error_count);
            ProtocolParser_ResetWorkingState(parser);
            result = PROTOCOL_PARSE_FORMAT_ERROR;
            break;
    }

    return result;
}

/**
 * @brief Encode one frame into a caller-owned output buffer.
 * @param message_id Message identifier.
 * @param flags Frame flags.
 * @param sequence Sequence value.
 * @param payload Payload pointer, or NULL when payload_length is zero.
 * @param payload_length Payload length.
 * @param[out] output Output buffer.
 * @param output_capacity Output buffer capacity.
 * @param[out] encoded_length Number of encoded bytes.
 * @return True on success.
 */
bool Protocol_EncodeFrame(uint16_t message_id,
                          uint8_t flags,
                          uint16_t sequence,
                          const uint8_t *payload,
                          uint8_t payload_length,
                          uint8_t *output,
                          size_t output_capacity,
                          size_t *encoded_length)
{
    size_t output_index;
    size_t payload_index;
    size_t required_length;
    uint16_t crc;

    if ((output == NULL) || (encoded_length == NULL))
    {
        return false;
    }

    if ((payload_length > PROTOCOL_MAX_PAYLOAD_LENGTH) ||
        ((payload_length > 0u) && (payload == NULL)))
    {
        return false;
    }

    required_length = PROTOCOL_FRAME_OVERHEAD_LENGTH + (size_t)payload_length;
    if (output_capacity < required_length)
    {
        return false;
    }

    output[0] = PROTOCOL_SOF_0;
    output[1] = PROTOCOL_SOF_1;
    output[2] = PROTOCOL_VERSION;
    output[3] = (uint8_t)(message_id & 0x00FFu);
    output[4] = (uint8_t)((message_id >> 8u) & 0x00FFu);
    output[5] = flags;
    output[6] = payload_length;
    output[7] = (uint8_t)(sequence & 0x00FFu);
    output[8] = (uint8_t)((sequence >> 8u) & 0x00FFu);

    crc = PROTOCOL_CRC_INITIAL_VALUE;
    output_index = 2u;
    while (output_index < 9u)
    {
        crc = ProtocolCrc_Update(crc, output[output_index]);
        output_index += 1u;
    }

    payload_index = 0u;
    while (payload_index < (size_t)payload_length)
    {
        output[9u + payload_index] = payload[payload_index];
        crc = ProtocolCrc_Update(crc, payload[payload_index]);
        payload_index += 1u;
    }

    output[9u + payload_length] = (uint8_t)(crc & 0x00FFu);
    output[10u + payload_length] = (uint8_t)((crc >> 8u) & 0x00FFu);
    *encoded_length = required_length;

    return true;
}

/**
 * @brief Write a little-endian 16-bit value.
 * @param destination Two-byte destination.
 * @param value Value to write.
 */
void Protocol_WriteU16Le(uint8_t *destination, uint16_t value)
{
    if (destination != NULL)
    {
        destination[0] = (uint8_t)(value & 0x00FFu);
        destination[1] = (uint8_t)((value >> 8u) & 0x00FFu);
    }
}

/**
 * @brief Write a little-endian 32-bit value.
 * @param destination Four-byte destination.
 * @param value Value to write.
 */
void Protocol_WriteU32Le(uint8_t *destination, uint32_t value)
{
    if (destination != NULL)
    {
        destination[0] = (uint8_t)(value & 0x000000FFu);
        destination[1] = (uint8_t)((value >> 8u) & 0x000000FFu);
        destination[2] = (uint8_t)((value >> 16u) & 0x000000FFu);
        destination[3] = (uint8_t)((value >> 24u) & 0x000000FFu);
    }
}
