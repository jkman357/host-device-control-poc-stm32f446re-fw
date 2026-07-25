// Copyright (c) 2026 Ray Yang. All rights reserved.
//
// File:
//     protocol.c
//
// Purpose:
//     Implements bounded authoritative frame parsing and serialization.
//
// Responsibilities:
//     - Maintains parser state in caller-owned storage.
//     - Validates payload length and CRC before delivering frames.
//     - Preserves unsupported versions for application-level NACK handling.
//     - Encodes frames without structure casting or dynamic allocation.

#include "protocol.h"

#include <float.h>
#include <limits.h>

#include "protocol_crc.h"

#define HEADER_INDEX_VERSION                 (0u)
#define HEADER_INDEX_MESSAGE_ID              (1u)
#define HEADER_INDEX_SEQUENCE_LOW            (2u)
#define HEADER_INDEX_SEQUENCE_HIGH           (3u)
#define HEADER_INDEX_PAYLOAD_LENGTH_LOW      (4u)
#define HEADER_INDEX_PAYLOAD_LENGTH_HIGH     (5u)

#define FRAME_INDEX_SOF_0                     (0u)
#define FRAME_INDEX_SOF_1                     (1u)
#define FRAME_INDEX_VERSION                   (2u)
#define FRAME_INDEX_MESSAGE_ID                (3u)
#define FRAME_INDEX_SEQUENCE_LOW              (4u)
#define FRAME_INDEX_SEQUENCE_HIGH             (5u)
#define FRAME_INDEX_PAYLOAD_LENGTH_LOW        (6u)
#define FRAME_INDEX_PAYLOAD_LENGTH_HIGH       (7u)
#define FRAME_INDEX_PAYLOAD                   (8u)

#define U16_LOW_BYTE_INDEX                    (0u)
#define U16_HIGH_BYTE_INDEX                   (1u)
#define U32_BYTE_0_INDEX                      (0u)
#define U32_BYTE_1_INDEX                      (1u)
#define U32_BYTE_2_INDEX                      (2u)
#define U32_BYTE_3_INDEX                      (3u)
#define FLOAT32_BYTE_COUNT                    (4u)
#define BYTE_SHIFT_BITS                       (8u)
#define U16_BYTE_MASK                         (0x00FFu)
#define U32_BYTE_MASK                         (0x000000FFu)

_Static_assert(sizeof(float) == FLOAT32_BYTE_COUNT, "Protocol requires 32-bit float.");
_Static_assert(FLT_RADIX == 2, "Protocol requires binary floating point.");
_Static_assert(FLT_MANT_DIG == 24, "Protocol requires IEEE-754 binary32 precision.");

#if !defined(__BYTE_ORDER__) || !defined(__ORDER_LITTLE_ENDIAN__)
#error Compiler byte-order definitions are required.
#endif

#if (__BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__)
#error This protocol implementation supports little-endian targets only.
#endif

/*
 * Function:
 *     protocol_increment_saturating_u32
 *
 * Purpose:
 *     Increments a diagnostic counter without unsigned wraparound.
 *
 * Input Parameters:
 *     counter:
 *         Points to the counter to update.
 *
 * Output Parameters:
 *     counter:
 *         Receives the incremented value or remains UINT32_MAX.
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
 *     Resets parser working state while preserving diagnostics.
 *
 * Input Parameters:
 *     parser:
 *         Points to the parser state to reset.
 *
 * Output Parameters:
 *     parser:
 *         Receives the initial working state.
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
    parser->partial_frame_elapsed_us = 0u;
}

/*
 * Function:
 *     protocol_parser_copy_header
 *
 * Purpose:
 *     Copies decoded header fields into a frame.
 *
 * Input Parameters:
 *     parser:
 *         Points to a parser containing a complete header.
 *
 * Output Parameters:
 *     frame:
 *         Receives version, message identifier, sequence, and payload length.
 *
 * Return Value:
 *     None.
 */
static void protocol_parser_copy_header(const protocol_parser_t *parser,
                                        protocol_frame_t *frame)
{
    frame->version = parser->header[HEADER_INDEX_VERSION];
    frame->message_id = parser->header[HEADER_INDEX_MESSAGE_ID];
    frame->sequence = (uint16_t)parser->header[HEADER_INDEX_SEQUENCE_LOW] |
                      (uint16_t)((uint16_t)parser->header[HEADER_INDEX_SEQUENCE_HIGH]
                                 << BYTE_SHIFT_BITS);
    frame->payload_length =
        (uint16_t)parser->header[HEADER_INDEX_PAYLOAD_LENGTH_LOW] |
        (uint16_t)((uint16_t)parser->header[HEADER_INDEX_PAYLOAD_LENGTH_HIGH]
                   << BYTE_SHIFT_BITS);
}

/*
 * Function:
 *     protocol_parser_init
 *
 * Purpose:
 *     Initializes a caller-owned protocol parser and clears diagnostics.
 *
 * Input Parameters:
 *     None.
 *
 * Output Parameters:
 *     parser:
 *         Receives initialized parser state when valid.
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
        parser->timeout_count = 0u;
        protocol_parser_reset_working_state(parser);
    }
}

/*
 * Function:
 *     protocol_parser_push_byte
 *
 * Purpose:
 *     Consumes one received byte and advances the caller-owned parser.
 *
 * Input Parameters:
 *     parser:
 *         Supplies the current parser state.
 *     data_byte:
 *         Supplies the next received wire byte.
 *
 * Output Parameters:
 *     parser:
 *         Receives updated parser state and diagnostics.
 *     frame:
 *         Receives a complete frame only when FRAME_READY is returned.
 *
 * Return Value:
 *     PROTOCOL_PARSE_NO_FRAME:
 *         No complete frame is available.
 *     PROTOCOL_PARSE_FRAME_READY:
 *         A complete CRC-valid frame is available.
 *     PROTOCOL_PARSE_FORMAT_ERROR:
 *         An argument or payload length was invalid.
 *     PROTOCOL_PARSE_CRC_ERROR:
 *         The candidate CRC did not match.
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
                parser->partial_frame_elapsed_us = 0u;
            }
            break;

        case PROTOCOL_PARSER_WAIT_SOF_1:
            parser->partial_frame_elapsed_us = 0u;
            if (data_byte == PROTOCOL_SOF_1)
            {
                parser->state = PROTOCOL_PARSER_READ_HEADER;
                parser->header_index = 0u;
                parser->calculated_crc = PROTOCOL_CRC_INITIAL_VALUE;
            }
            else if (data_byte != PROTOCOL_SOF_0)
            {
                protocol_parser_reset_working_state(parser);
            }
            else
            {
                // A repeated first SOF byte remains a valid resynchronization candidate.
            }
            break;

        case PROTOCOL_PARSER_READ_HEADER:
            parser->partial_frame_elapsed_us = 0u;
            parser->header[parser->header_index] = data_byte;
            parser->calculated_crc = protocol_crc_update(parser->calculated_crc, data_byte);

            if (parser->header_index < (PROTOCOL_FIXED_HEADER_LENGTH - 1u))
            {
                parser->header_index += 1u;
            }
            else
            {
                protocol_parser_copy_header(parser, frame);
                parser->payload_index = 0u;

                if (frame->payload_length > PROTOCOL_MAX_PAYLOAD_LENGTH)
                {
                    protocol_increment_saturating_u32(&parser->format_error_count);
                    protocol_parser_reset_working_state(parser);
                    result = PROTOCOL_PARSE_FORMAT_ERROR;
                }
                else if (frame->payload_length == 0u)
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
            parser->partial_frame_elapsed_us = 0u;
            frame->payload[parser->payload_index] = data_byte;
            parser->calculated_crc = protocol_crc_update(parser->calculated_crc, data_byte);

            if (parser->payload_index < (frame->payload_length - 1u))
            {
                parser->payload_index += 1u;
            }
            else
            {
                parser->state = PROTOCOL_PARSER_READ_CRC_LOW;
            }
            break;

        case PROTOCOL_PARSER_READ_CRC_LOW:
            parser->partial_frame_elapsed_us = 0u;
            parser->received_crc = (uint16_t)data_byte;
            parser->state = PROTOCOL_PARSER_READ_CRC_HIGH;
            break;

        case PROTOCOL_PARSER_READ_CRC_HIGH:
            parser->partial_frame_elapsed_us = 0u;
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
 *     protocol_parser_advance_time_us
 *
 * Purpose:
 *     Applies elapsed time to partial-frame timeout handling.
 *
 * Input Parameters:
 *     parser:
 *         Supplies the parser to update.
 *     elapsed_us:
 *         Supplies elapsed microseconds.
 *
 * Output Parameters:
 *     parser:
 *         Receives updated timeout state and diagnostics.
 *
 * Return Value:
 *     true:
 *         A partial frame timed out and was discarded.
 *     false:
 *         No timeout occurred or parser was NULL.
 */
bool protocol_parser_advance_time_us(protocol_parser_t *parser, uint32_t elapsed_us)
{
    uint32_t remaining_us;

    if ((parser == NULL) || (parser->state == PROTOCOL_PARSER_WAIT_SOF_0))
    {
        return false;
    }

    remaining_us = PROTOCOL_PARTIAL_FRAME_TIMEOUT_US - parser->partial_frame_elapsed_us;
    if (elapsed_us >= remaining_us)
    {
        protocol_increment_saturating_u32(&parser->timeout_count);
        protocol_parser_reset_working_state(parser);
        return true;
    }

    parser->partial_frame_elapsed_us += elapsed_us;
    return false;
}

/*
 * Function:
 *     protocol_encode_frame
 *
 * Purpose:
 *     Validates and encodes one authoritative protocol frame.
 *
 * Input Parameters:
 *     message_id:
 *         Supplies the one-byte message identifier.
 *     sequence:
 *         Supplies the frame sequence.
 *     payload:
 *         Points to payload bytes or is NULL for an empty payload.
 *     payload_length:
 *         Supplies the payload byte count.
 *     output_capacity:
 *         Supplies the output buffer capacity.
 *
 * Output Parameters:
 *     output:
 *         Receives the complete encoded frame on success.
 *     encoded_length:
 *         Receives the encoded byte count on success.
 *
 * Return Value:
 *     true:
 *         Encoding succeeded.
 *     false:
 *         An argument, length, or capacity was invalid.
 */
bool protocol_encode_frame(uint8_t message_id,
                           uint16_t sequence,
                           const uint8_t *payload,
                           uint16_t payload_length,
                           uint8_t *output,
                           size_t output_capacity,
                           size_t *encoded_length)
{
    size_t body_index;
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
    output[FRAME_INDEX_MESSAGE_ID] = message_id;
    output[FRAME_INDEX_SEQUENCE_LOW] = (uint8_t)(sequence & U16_BYTE_MASK);
    output[FRAME_INDEX_SEQUENCE_HIGH] =
        (uint8_t)((sequence >> BYTE_SHIFT_BITS) & U16_BYTE_MASK);
    output[FRAME_INDEX_PAYLOAD_LENGTH_LOW] =
        (uint8_t)(payload_length & U16_BYTE_MASK);
    output[FRAME_INDEX_PAYLOAD_LENGTH_HIGH] =
        (uint8_t)((payload_length >> BYTE_SHIFT_BITS) & U16_BYTE_MASK);

    crc = PROTOCOL_CRC_INITIAL_VALUE;
    body_index = FRAME_INDEX_VERSION;
    while (body_index < FRAME_INDEX_PAYLOAD)
    {
        crc = protocol_crc_update(crc, output[body_index]);
        body_index += 1u;
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
 *     protocol_read_u16_le
 *
 * Purpose:
 *     Reads one little-endian 16-bit value.
 *
 * Input Parameters:
 *     source:
 *         Points to at least two readable bytes.
 *
 * Output Parameters:
 *     None.
 *
 * Return Value:
 *     result:
 *         Decoded value, or zero when source is NULL.
 */
uint16_t protocol_read_u16_le(const uint8_t *source)
{
    if (source == NULL)
    {
        return 0u;
    }

    return (uint16_t)source[U16_LOW_BYTE_INDEX] |
           (uint16_t)((uint16_t)source[U16_HIGH_BYTE_INDEX] << BYTE_SHIFT_BITS);
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
 *         Receives two serialized bytes when valid.
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
 *         Receives four serialized bytes when valid.
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

/*
 * Function:
 *     protocol_write_float32_le
 *
 * Purpose:
 *     Writes one IEEE-754 binary32 value in little-endian byte order.
 *
 * Input Parameters:
 *     value:
 *         Supplies the float value to serialize.
 *
 * Output Parameters:
 *     destination:
 *         Receives four serialized bytes when valid.
 *
 * Return Value:
 *     None.
 */
void protocol_write_float32_le(uint8_t *destination, float value)
{
    const unsigned char *source_bytes;
    size_t byte_index;

    if (destination != NULL)
    {
        source_bytes = (const unsigned char *)(const void *)&value;
        byte_index = 0u;
        while (byte_index < FLOAT32_BYTE_COUNT)
        {
            destination[byte_index] = source_bytes[byte_index];
            byte_index += 1u;
        }
    }
}
