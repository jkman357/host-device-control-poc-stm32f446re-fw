// Copyright (c) 2026 Ray Yang. All rights reserved.
//
// File:
//     protocol.c
//
// Purpose:
//     Implements bounded frame parsing and serialization.
//
// Responsibilities:
//     - Maintains parser state in caller-owned storage.
//     - Validates version, payload length, and CRC.
//     - Encodes frames without structure casting or dynamic allocation.
//     - Writes little-endian scalar fields explicitly.

#include "protocol.h"

#include <limits.h>

#include "protocol_crc.h"

#define HEADER_INDEX_VERSION             (0u)
#define HEADER_INDEX_MESSAGE_ID_LOW      (1u)
#define HEADER_INDEX_MESSAGE_ID_HIGH     (2u)
#define HEADER_INDEX_FLAGS               (3u)
#define HEADER_INDEX_PAYLOAD_LENGTH      (4u)
#define HEADER_INDEX_SEQUENCE_LOW        (5u)
#define HEADER_INDEX_SEQUENCE_HIGH       (6u)

#define FRAME_INDEX_SOF_0                 (0u)
#define FRAME_INDEX_SOF_1                 (1u)
#define FRAME_INDEX_VERSION               (2u)
#define FRAME_INDEX_MESSAGE_ID_LOW        (3u)
#define FRAME_INDEX_MESSAGE_ID_HIGH       (4u)
#define FRAME_INDEX_FLAGS                 (5u)
#define FRAME_INDEX_PAYLOAD_LENGTH        (6u)
#define FRAME_INDEX_SEQUENCE_LOW          (7u)
#define FRAME_INDEX_SEQUENCE_HIGH         (8u)
#define FRAME_INDEX_PAYLOAD               (9u)

#define U16_LOW_BYTE_INDEX                (0u)
#define U16_HIGH_BYTE_INDEX               (1u)
#define U32_BYTE_0_INDEX                  (0u)
#define U32_BYTE_1_INDEX                  (1u)
#define U32_BYTE_2_INDEX                  (2u)
#define U32_BYTE_3_INDEX                  (3u)
#define BYTE_SHIFT_BITS                   (8u)
#define U16_BYTE_MASK                     (0x00FFu)
#define U32_BYTE_MASK                     (0x000000FFu)

/*
 * Function:
 *     protocol_increment_saturating_u32
 *
 * Purpose:
 *     Increments a parser diagnostic counter without allowing unsigned wraparound.
 *
 * Input Parameters:
 *     counter:
 *         Points to the parser-owned counter to update.
 *
 * Output Parameters:
 *     counter:
 *         Receives the incremented value or remains UINT32_MAX when saturated.
 *
 * Return Value:
 *     None.
 */
static void protocol_increment_saturating_u32(uint32_t *counter)
{
    if (*counter < UINT32_MAX)
    {
        *counter += 1u;
    }
}

/*
 * Function:
 *     protocol_parser_reset_working_state
 *
 * Purpose:
 *     Resets parser working state while preserving diagnostic counters.
 *
 * Input Parameters:
 *     parser:
 *         Points to the parser state to reset.
 *
 * Output Parameters:
 *     parser:
 *         Receives the initial working state while its diagnostic counters remain unchanged.
 *
 * Return Value:
 *     None.
 */
static void protocol_parser_reset_working_state(protocol_parser_t *parser)
{
    parser->state = PROTOCOL_PARSER_WAIT_SOF_0;
    parser->header_index = 0u;
    parser->payload_index = 0u;
    parser->calculated_crc = PROTOCOL_CRC_INITIAL_VALUE;
    parser->received_crc = 0u;
}

/*
 * Function:
 *     protocol_parser_copy_header
 *
 * Purpose:
 *     Copies decoded header fields from parser storage into a frame.
 *
 * Input Parameters:
 *     parser:
 *         Points to the parser containing a complete validated header.
 *
 * Output Parameters:
 *     frame:
 *         Receives the decoded message identifier, flags, payload length, and sequence.
 *
 * Return Value:
 *     None.
 */
static void protocol_parser_copy_header(const protocol_parser_t *parser,
                                        protocol_frame_t *frame)
{
    frame->message_id = (uint16_t)parser->header[HEADER_INDEX_MESSAGE_ID_LOW] |
                        (uint16_t)((uint16_t)parser->header[HEADER_INDEX_MESSAGE_ID_HIGH]
                                   << BYTE_SHIFT_BITS);
    frame->flags = parser->header[HEADER_INDEX_FLAGS];
    frame->payload_length = parser->header[HEADER_INDEX_PAYLOAD_LENGTH];
    frame->sequence = (uint16_t)parser->header[HEADER_INDEX_SEQUENCE_LOW] |
                      (uint16_t)((uint16_t)parser->header[HEADER_INDEX_SEQUENCE_HIGH]
                                 << BYTE_SHIFT_BITS);
}

/*
 * Function:
 *     protocol_parser_init
 *
 * Purpose:
 *     Initializes a caller-owned protocol parser and clears its diagnostics.
 *
 * Input Parameters:
 *     None.
 *
 * Output Parameters:
 *     parser:
 *         Receives initialized parser state when the pointer is valid. A NULL pointer is ignored.
 *
 * Return Value:
 *     None.
 */
void protocol_parser_init(protocol_parser_t *parser)
{
    if (parser != NULL)
    {
        parser->format_error_count = 0u;
        parser->crc_error_count = 0u;
        protocol_parser_reset_working_state(parser);
    }
}

/*
 * Function:
 *     protocol_parser_push_byte
 *
 * Purpose:
 *     Consumes one received byte and advances a caller-owned frame parser.
 *
 * Input Parameters:
 *     parser:
 *         Supplies the current parser state.
 *     data_byte:
 *         Supplies the next received wire byte.
 *
 * Output Parameters:
 *     parser:
 *         Receives updated parser state and diagnostic counters.
 *     frame:
 *         Receives a completed frame only when PROTOCOL_PARSE_FRAME_READY is returned.
 *
 * Return Value:
 *     PROTOCOL_PARSE_NO_FRAME:
 *         No complete frame is available.
 *     PROTOCOL_PARSE_FRAME_READY:
 *         A complete CRC-valid frame is available in frame.
 *     PROTOCOL_PARSE_FORMAT_ERROR:
 *         An argument or header field was invalid.
 *     PROTOCOL_PARSE_CRC_ERROR:
 *         The candidate frame CRC did not match.
 *
 * Notes:
 *     Runs in main context and performs bounded work for one input byte.
 */
protocol_parse_result_t protocol_parser_push_byte(protocol_parser_t *parser,
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
                // Keep waiting for SOF_1 after a repeated SOF_0.
            }
            break;

        case PROTOCOL_PARSER_READ_HEADER:
            parser->header[parser->header_index] = data_byte;
            parser->calculated_crc = protocol_crc_update(parser->calculated_crc, data_byte);

            if (parser->header_index < (PROTOCOL_FIXED_HEADER_LENGTH - 1u))
            {
                parser->header_index += 1u;
            }
            else if ((parser->header[HEADER_INDEX_VERSION] != PROTOCOL_VERSION) ||
                     (parser->header[HEADER_INDEX_PAYLOAD_LENGTH] > PROTOCOL_MAX_PAYLOAD_LENGTH))
            {
                protocol_increment_saturating_u32(&parser->format_error_count);
                protocol_parser_reset_working_state(parser);
                result = PROTOCOL_PARSE_FORMAT_ERROR;
            }
            else
            {
                protocol_parser_copy_header(parser, frame);
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
            break;

        case PROTOCOL_PARSER_READ_PAYLOAD:
            frame->payload[parser->payload_index] = data_byte;
            parser->calculated_crc = protocol_crc_update(parser->calculated_crc, data_byte);

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
            parser->received_crc |= (uint16_t)((uint16_t)data_byte << BYTE_SHIFT_BITS);

            if (parser->received_crc == parser->calculated_crc)
            {
                result = PROTOCOL_PARSE_FRAME_READY;
            }
            else
            {
                protocol_increment_saturating_u32(&parser->crc_error_count);
                result = PROTOCOL_PARSE_CRC_ERROR;
            }

            protocol_parser_reset_working_state(parser);
            break;

        default:
            protocol_increment_saturating_u32(&parser->format_error_count);
            protocol_parser_reset_working_state(parser);
            result = PROTOCOL_PARSE_FORMAT_ERROR;
            break;
    }

    return result;
}

/*
 * Function:
 *     protocol_encode_frame
 *
 * Purpose:
 *     Validates and encodes one protocol frame into caller-owned storage.
 *
 * Input Parameters:
 *     message_id:
 *         Supplies the message identifier.
 *     flags:
 *         Supplies the frame classification flags.
 *     sequence:
 *         Supplies the frame sequence value.
 *     payload:
 *         Points to payload bytes, or is NULL when payload_length is zero.
 *     payload_length:
 *         Supplies the number of payload bytes.
 *     output_capacity:
 *         Supplies the output buffer capacity.
 *
 * Output Parameters:
 *     output:
 *         Receives the complete encoded frame only when true is returned.
 *     encoded_length:
 *         Receives the encoded byte count only when true is returned.
 *
 * Return Value:
 *     true:
 *         The frame was encoded successfully.
 *     false:
 *         An argument, payload length, or output capacity was invalid.
 *
 * Notes:
 *     The function does not retain caller-owned pointers. Output objects are unspecified on failure.
 */
bool protocol_encode_frame(uint16_t message_id,
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

    output[FRAME_INDEX_SOF_0] = PROTOCOL_SOF_0;
    output[FRAME_INDEX_SOF_1] = PROTOCOL_SOF_1;
    output[FRAME_INDEX_VERSION] = PROTOCOL_VERSION;
    output[FRAME_INDEX_MESSAGE_ID_LOW] = (uint8_t)(message_id & U16_BYTE_MASK);
    output[FRAME_INDEX_MESSAGE_ID_HIGH] =
        (uint8_t)((message_id >> BYTE_SHIFT_BITS) & U16_BYTE_MASK);
    output[FRAME_INDEX_FLAGS] = flags;
    output[FRAME_INDEX_PAYLOAD_LENGTH] = payload_length;
    output[FRAME_INDEX_SEQUENCE_LOW] = (uint8_t)(sequence & U16_BYTE_MASK);
    output[FRAME_INDEX_SEQUENCE_HIGH] =
        (uint8_t)((sequence >> BYTE_SHIFT_BITS) & U16_BYTE_MASK);

    crc = PROTOCOL_CRC_INITIAL_VALUE;
    output_index = FRAME_INDEX_VERSION;
    while (output_index < FRAME_INDEX_PAYLOAD)
    {
        crc = protocol_crc_update(crc, output[output_index]);
        output_index += 1u;
    }

    payload_index = 0u;
    while (payload_index < (size_t)payload_length)
    {
        output[FRAME_INDEX_PAYLOAD + payload_index] = payload[payload_index];
        crc = protocol_crc_update(crc, payload[payload_index]);
        payload_index += 1u;
    }

    output[FRAME_INDEX_PAYLOAD + payload_length] = (uint8_t)(crc & U16_BYTE_MASK);
    output[FRAME_INDEX_PAYLOAD + payload_length + 1u] =
        (uint8_t)((crc >> BYTE_SHIFT_BITS) & U16_BYTE_MASK);
    *encoded_length = required_length;

    return true;
}

/*
 * Function:
 *     protocol_write_u16_le
 *
 * Purpose:
 *     Writes one 16-bit value in little-endian byte order.
 *
 * Input Parameters:
 *     value:
 *         Supplies the value to serialize.
 *
 * Output Parameters:
 *     destination:
 *         Receives two serialized bytes when the pointer is valid. A NULL pointer is ignored.
 *
 * Return Value:
 *     None.
 */
void protocol_write_u16_le(uint8_t *destination, uint16_t value)
{
    if (destination != NULL)
    {
        destination[U16_LOW_BYTE_INDEX] = (uint8_t)(value & U16_BYTE_MASK);
        destination[U16_HIGH_BYTE_INDEX] =
            (uint8_t)((value >> BYTE_SHIFT_BITS) & U16_BYTE_MASK);
    }
}

/*
 * Function:
 *     protocol_write_u32_le
 *
 * Purpose:
 *     Writes one 32-bit value in little-endian byte order.
 *
 * Input Parameters:
 *     value:
 *         Supplies the value to serialize.
 *
 * Output Parameters:
 *     destination:
 *         Receives four serialized bytes when the pointer is valid. A NULL pointer is ignored.
 *
 * Return Value:
 *     None.
 */
void protocol_write_u32_le(uint8_t *destination, uint32_t value)
{
    if (destination != NULL)
    {
        destination[U32_BYTE_0_INDEX] = (uint8_t)(value & U32_BYTE_MASK);
        destination[U32_BYTE_1_INDEX] =
            (uint8_t)((value >> BYTE_SHIFT_BITS) & U32_BYTE_MASK);
        destination[U32_BYTE_2_INDEX] =
            (uint8_t)((value >> (BYTE_SHIFT_BITS * 2u)) & U32_BYTE_MASK);
        destination[U32_BYTE_3_INDEX] =
            (uint8_t)((value >> (BYTE_SHIFT_BITS * 3u)) & U32_BYTE_MASK);
    }
}
