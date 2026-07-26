// Copyright (c) 2026 Ray Yang. All rights reserved.
//
// File:
//     protocol.c
//
// Purpose:
//     Implements the bounded binary protocol codec and incremental parser.
//
// Responsibilities:
//     - Calculates CRC-16/CCITT-FALSE checksums.
//     - Encodes frames into caller-owned bounded buffers.
//     - Parses frames incrementally with length, CRC, and timeout validation.
//     - Serializes integer and floating-point fields explicitly in little-endian order.
//
// Notes:
//     The module owns no dynamic memory and performs no transport input or output.

#include "protocol.h"

#include <float.h>
#include <limits.h>

#define CRC_INITIAL (0xFFFFu)
#define CRC_POLYNOMIAL (0x1021u)
#define CRC_MSB_MASK (0x8000u)
#define CRC_BIT_COUNT_PER_BYTE (8u)

#define BYTE_SHIFT_8 (8u)
#define BYTE_SHIFT_16 (16u)
#define BYTE_SHIFT_24 (24u)

#define HEADER_VERSION_OFFSET (0u)
#define HEADER_MESSAGE_ID_OFFSET (1u)
#define HEADER_SEQUENCE_OFFSET (2u)
#define HEADER_PAYLOAD_LENGTH_OFFSET (4u)

#define FRAME_SOF_0_OFFSET (0u)
#define FRAME_SOF_1_OFFSET (1u)
#define FRAME_VERSION_OFFSET (2u)
#define FRAME_MESSAGE_ID_OFFSET (3u)
#define FRAME_SEQUENCE_OFFSET (4u)
#define FRAME_PAYLOAD_LENGTH_OFFSET (6u)
#define FRAME_PAYLOAD_OFFSET (8u)
#define FRAME_CRC_INPUT_OFFSET FRAME_VERSION_OFFSET
#define FRAME_CRC_HEADER_LENGTH (6u)

_Static_assert(sizeof(float) == sizeof(uint32_t),
               "Protocol float32 requires a 32-bit float object representation.");
_Static_assert((FLT_RADIX == 2) && (FLT_MANT_DIG == 24) && (FLT_MAX_EXP == 128),
               "Protocol float32 requires an IEEE-754 binary32 representation.");

/*
 * Function:
 *     protocol_increment_saturating
 *
 * Purpose:
 *     Increments an error counter without permitting unsigned wraparound.
 *
 * Input Parameters:
 *     value:
 *         Pointer to the counter to inspect and update.
 *
 * Output Parameters:
 *     value:
 *         Receives the incremented value unless already saturated.
 *
 * Return Value:
 *     None.
 */
static void protocol_increment_saturating(uint32_t *value)
{
    if (*value < UINT32_MAX)
    {
        *value += 1u;
    }
}

/*
 * Function:
 *     protocol_crc_update
 *
 * Purpose:
 *     Updates a CRC-16/CCITT-FALSE accumulator with one byte.
 *
 * Input Parameters:
 *     crc:
 *         Current CRC accumulator.
 *     data_byte:
 *         Next byte to include.
 *
 * Output Parameters:
 *     None.
 *
 * Return Value:
 *     Updated CRC accumulator.
 */
static uint16_t protocol_crc_update(uint16_t crc, uint8_t data_byte)
{
    uint8_t bit_index;

    crc ^= (uint16_t)((uint16_t)data_byte << BYTE_SHIFT_8);
    for (bit_index = 0u; bit_index < CRC_BIT_COUNT_PER_BYTE; bit_index += 1u)
    {
        if ((crc & CRC_MSB_MASK) != 0u)
        {
            crc = (uint16_t)(((uint32_t)crc << 1u) ^ (uint32_t)CRC_POLYNOMIAL);
        }
        else
        {
            crc = (uint16_t)((uint32_t)crc << 1u);
        }
    }

    return crc;
}

/*
 * Function:
 *     protocol_parser_reset_candidate
 *
 * Purpose:
 *     Discards the current partial frame while retaining accumulated error counters.
 *
 * Input Parameters:
 *     parser:
 *         Pointer to parser state to reset.
 *
 * Output Parameters:
 *     parser:
 *         Receives the wait-for-SOF candidate state.
 *
 * Return Value:
 *     None.
 */
static void protocol_parser_reset_candidate(protocol_parser_t *parser)
{
    parser->state = PROTOCOL_PARSER_WAIT_SOF_0;
    parser->header_index = 0u;
    parser->payload_index = 0u;
    parser->calculated_crc = CRC_INITIAL;
    parser->received_crc = 0u;
    parser->partial_frame_elapsed_us = 0u;
}

/*
 * Function:
 *     protocol_parser_init
 *
 * Purpose:
 *     Initializes a caller-owned incremental parser and clears all error counters.
 *
 * Input Parameters:
 *     parser:
 *         Pointer to parser storage to initialize.
 *
 * Output Parameters:
 *     parser:
 *         Receives the initialized parser state when non-NULL.
 *
 * Return Value:
 *     None.
 */
void protocol_parser_init(protocol_parser_t *parser)
{
    if (parser == NULL)
    {
        return;
    }

    parser->format_error_count = 0u;
    parser->crc_error_count = 0u;
    parser->timeout_count = 0u;
    protocol_parser_reset_candidate(parser);
}

/*
 * Function:
 *     protocol_crc16_ccitt_false
 *
 * Purpose:
 *     Calculates CRC-16/CCITT-FALSE over a bounded byte sequence.
 *
 * Input Parameters:
 *     data:
 *         Pointer to input bytes; may be NULL only when length is zero.
 *     length:
 *         Number of input bytes.
 *
 * Output Parameters:
 *     None.
 *
 * Return Value:
 *     Calculated CRC value.
 */
uint16_t protocol_crc16_ccitt_false(const uint8_t *data, size_t length)
{
    uint16_t crc = CRC_INITIAL;
    size_t data_index;

    if ((data == NULL) && (length != 0u))
    {
        return 0u;
    }

    for (data_index = 0u; data_index < length; data_index += 1u)
    {
        crc = protocol_crc_update(crc, data[data_index]);
    }

    return crc;
}

/*
 * Function:
 *     protocol_read_u16_le
 *
 * Purpose:
 *     Reads one 16-bit unsigned little-endian value from a byte buffer.
 *
 * Input Parameters:
 *     source:
 *         Pointer to at least two readable bytes.
 *
 * Output Parameters:
 *     None.
 *
 * Return Value:
 *     Decoded 16-bit value.
 */
uint16_t protocol_read_u16_le(const uint8_t *source)
{
    if (source == NULL)
    {
        return 0u;
    }

    return (uint16_t)((uint16_t)source[0] | ((uint16_t)source[1] << BYTE_SHIFT_8));
}

/*
 * Function:
 *     protocol_read_u32_le
 *
 * Purpose:
 *     Reads one 32-bit unsigned little-endian value from a byte buffer.
 *
 * Input Parameters:
 *     source:
 *         Pointer to at least four readable bytes.
 *
 * Output Parameters:
 *     None.
 *
 * Return Value:
 *     Decoded 32-bit value.
 */
uint32_t protocol_read_u32_le(const uint8_t *source)
{
    if (source == NULL)
    {
        return 0u;
    }

    return (uint32_t)source[0]
         | ((uint32_t)source[1] << BYTE_SHIFT_8)
         | ((uint32_t)source[2] << BYTE_SHIFT_16)
         | ((uint32_t)source[3] << BYTE_SHIFT_24);
}

/*
 * Function:
 *     protocol_write_u16_le
 *
 * Purpose:
 *     Writes one 16-bit unsigned value in little-endian byte order.
 *
 * Input Parameters:
 *     destination:
 *         Pointer to at least two writable bytes.
 *     value:
 *         Value to serialize.
 *
 * Output Parameters:
 *     destination:
 *         Receives two serialized bytes.
 *
 * Return Value:
 *     None.
 */
void protocol_write_u16_le(uint8_t *destination, uint16_t value)
{
    if (destination != NULL)
    {
        destination[0] = (uint8_t)value;
        destination[1] = (uint8_t)(value >> BYTE_SHIFT_8);
    }
}

/*
 * Function:
 *     protocol_write_u32_le
 *
 * Purpose:
 *     Writes one 32-bit unsigned value in little-endian byte order.
 *
 * Input Parameters:
 *     destination:
 *         Pointer to at least four writable bytes.
 *     value:
 *         Value to serialize.
 *
 * Output Parameters:
 *     destination:
 *         Receives four serialized bytes.
 *
 * Return Value:
 *     None.
 */
void protocol_write_u32_le(uint8_t *destination, uint32_t value)
{
    if (destination != NULL)
    {
        destination[0] = (uint8_t)value;
        destination[1] = (uint8_t)(value >> BYTE_SHIFT_8);
        destination[2] = (uint8_t)(value >> BYTE_SHIFT_16);
        destination[3] = (uint8_t)(value >> BYTE_SHIFT_24);
    }
}

/*
 * Function:
 *     protocol_float32_bits
 *
 * Purpose:
 *     Copies a verified float32 object representation into an unsigned integer.
 *
 * Input Parameters:
 *     value:
 *         Floating-point value whose object representation is required.
 *
 * Output Parameters:
 *     None.
 *
 * Return Value:
 *     Unsigned integer containing the identical IEEE-754 binary32 bit pattern.
 *
 * Notes:
 *     Character-type access is used to avoid prohibited union type-punning.
 */
static uint32_t protocol_float32_bits(float value)
{
    uint32_t value_bits = 0u;
    const uint8_t *source_bytes = (const uint8_t *)&value;
    uint8_t *destination_bytes = (uint8_t *)&value_bits;
    size_t byte_index;

    for (byte_index = 0u; byte_index < sizeof(value_bits); byte_index += 1u)
    {
        destination_bytes[byte_index] = source_bytes[byte_index];
    }

    return value_bits;
}

/*
 * Function:
 *     protocol_write_float32_le
 *
 * Purpose:
 *     Writes one verified 32-bit IEEE-754 floating-point value in little-endian order.
 *
 * Input Parameters:
 *     destination:
 *         Pointer to at least four writable bytes.
 *     value:
 *         Floating-point value to serialize.
 *
 * Output Parameters:
 *     destination:
 *         Receives four serialized bytes.
 *
 * Return Value:
 *     None.
 *
 * Notes:
 *     The build statically requires a 32-bit float representation.
 */
void protocol_write_float32_le(uint8_t *destination, float value)
{
    if (destination != NULL)
    {
        protocol_write_u32_le(destination, protocol_float32_bits(value));
    }
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
 *         Protocol message identifier.
 *     sequence:
 *         Protocol sequence number.
 *     payload:
 *         Pointer to payload bytes, or NULL for a zero-length payload.
 *     payload_length:
 *         Number of payload bytes.
 *     output:
 *         Pointer to caller-owned output buffer.
 *     output_capacity:
 *         Available bytes in output.
 *     encoded_length:
 *         Pointer to storage for the encoded byte count.
 *
 * Output Parameters:
 *     output:
 *         Receives the encoded frame on success.
 *     encoded_length:
 *         Receives the encoded frame length on success.
 *
 * Return Value:
 *     true:
 *         The frame was encoded successfully.
 *     false:
 *         An argument, length, or capacity check failed.
 */
bool protocol_encode_frame(uint8_t message_id,
                           uint16_t sequence,
                           const uint8_t *payload,
                           uint16_t payload_length,
                           uint8_t *output,
                           size_t output_capacity,
                           size_t *encoded_length)
{
    const size_t total_length = (size_t)payload_length + PROTOCOL_FRAME_OVERHEAD_LENGTH;
    size_t payload_index;
    uint16_t crc;

    if ((output == NULL) || (encoded_length == NULL))
    {
        return false;
    }
    if (payload_length > PROTOCOL_MAX_PAYLOAD_LENGTH)
    {
        return false;
    }
    if (output_capacity < total_length)
    {
        return false;
    }
    if ((payload_length != 0u) && (payload == NULL))
    {
        return false;
    }

    output[FRAME_SOF_0_OFFSET] = PROTOCOL_SOF_0;
    output[FRAME_SOF_1_OFFSET] = PROTOCOL_SOF_1;
    output[FRAME_VERSION_OFFSET] = PROTOCOL_VERSION;
    output[FRAME_MESSAGE_ID_OFFSET] = message_id;
    protocol_write_u16_le(&output[FRAME_SEQUENCE_OFFSET], sequence);
    protocol_write_u16_le(&output[FRAME_PAYLOAD_LENGTH_OFFSET], payload_length);

    for (payload_index = 0u; payload_index < payload_length; payload_index += 1u)
    {
        output[FRAME_PAYLOAD_OFFSET + payload_index] = payload[payload_index];
    }

    crc = protocol_crc16_ccitt_false(
        &output[FRAME_CRC_INPUT_OFFSET],
        (size_t)FRAME_CRC_HEADER_LENGTH + payload_length);
    protocol_write_u16_le(&output[FRAME_PAYLOAD_OFFSET + payload_length], crc);
    *encoded_length = total_length;
    return true;
}

/*
 * Function:
 *     protocol_parser_push_byte
 *
 * Purpose:
 *     Consumes one byte and advances the incremental frame parser.
 *
 * Input Parameters:
 *     parser:
 *         Pointer to parser state that is read and updated.
 *     data_byte:
 *         Next received wire byte.
 *     frame:
 *         Pointer to caller-owned decoded-frame storage.
 *
 * Output Parameters:
 *     parser:
 *         Receives the updated parser state and error counters.
 *     frame:
 *         Receives a validated frame only when FRAME_READY is returned.
 *
 * Return Value:
 *     PROTOCOL_PARSE_NO_FRAME:
 *         More bytes are required.
 *     PROTOCOL_PARSE_FRAME_READY:
 *         A complete validated frame was decoded.
 *     PROTOCOL_PARSE_FORMAT_ERROR:
 *         A structural field was invalid.
 *     PROTOCOL_PARSE_CRC_ERROR:
 *         The received CRC did not match.
 */
protocol_parse_result_t protocol_parser_push_byte(protocol_parser_t *parser,
                                                   uint8_t data_byte,
                                                   protocol_frame_t *frame)
{
    uint16_t payload_length;

    if ((parser == NULL) || (frame == NULL))
    {
        return PROTOCOL_PARSE_FORMAT_ERROR;
    }

    if (parser->state != PROTOCOL_PARSER_WAIT_SOF_0)
    {
        parser->partial_frame_elapsed_us = 0u;
    }

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
                parser->calculated_crc = CRC_INITIAL;
            }
            else if (data_byte != PROTOCOL_SOF_0)
            {
                parser->state = PROTOCOL_PARSER_WAIT_SOF_0;
            }
            break;

        case PROTOCOL_PARSER_READ_HEADER:
            parser->header[parser->header_index] = data_byte;
            parser->header_index += 1u;
            parser->calculated_crc = protocol_crc_update(parser->calculated_crc, data_byte);

            if (parser->header_index == PROTOCOL_FIXED_HEADER_LENGTH)
            {
                payload_length = protocol_read_u16_le(
                    &parser->header[HEADER_PAYLOAD_LENGTH_OFFSET]);
                if (payload_length > PROTOCOL_MAX_PAYLOAD_LENGTH)
                {
                    protocol_increment_saturating(&parser->format_error_count);
                    protocol_parser_reset_candidate(parser);
                    return PROTOCOL_PARSE_FORMAT_ERROR;
                }

                frame->version = parser->header[HEADER_VERSION_OFFSET];
                frame->message_id = parser->header[HEADER_MESSAGE_ID_OFFSET];
                frame->sequence = protocol_read_u16_le(
                    &parser->header[HEADER_SEQUENCE_OFFSET]);
                frame->payload_length = payload_length;
                parser->payload_index = 0u;
                parser->state = (payload_length == 0u)
                    ? PROTOCOL_PARSER_READ_CRC_LOW
                    : PROTOCOL_PARSER_READ_PAYLOAD;
            }
            break;

        case PROTOCOL_PARSER_READ_PAYLOAD:
            frame->payload[parser->payload_index] = data_byte;
            parser->payload_index += 1u;
            parser->calculated_crc = protocol_crc_update(parser->calculated_crc, data_byte);
            if (parser->payload_index == frame->payload_length)
            {
                parser->state = PROTOCOL_PARSER_READ_CRC_LOW;
            }
            break;

        case PROTOCOL_PARSER_READ_CRC_LOW:
            parser->received_crc = data_byte;
            parser->state = PROTOCOL_PARSER_READ_CRC_HIGH;
            break;

        case PROTOCOL_PARSER_READ_CRC_HIGH:
            parser->received_crc |= (uint16_t)((uint16_t)data_byte << BYTE_SHIFT_8);
            if (parser->received_crc == parser->calculated_crc)
            {
                protocol_parser_reset_candidate(parser);
                return PROTOCOL_PARSE_FRAME_READY;
            }

            protocol_increment_saturating(&parser->crc_error_count);
            protocol_parser_reset_candidate(parser);
            return PROTOCOL_PARSE_CRC_ERROR;

        default:
            protocol_increment_saturating(&parser->format_error_count);
            protocol_parser_reset_candidate(parser);
            return PROTOCOL_PARSE_FORMAT_ERROR;
    }

    return PROTOCOL_PARSE_NO_FRAME;
}

/*
 * Function:
 *     protocol_parser_advance_time_us
 *
 * Purpose:
 *     Advances partial-frame timeout accounting and resets an expired candidate.
 *
 * Input Parameters:
 *     parser:
 *         Pointer to parser state that is read and updated.
 *     elapsed_us:
 *         Elapsed time in microseconds since the prior update.
 *
 * Output Parameters:
 *     parser:
 *         Receives updated timeout state and timeout count.
 *
 * Return Value:
 *     true:
 *         A partial frame timed out and was discarded.
 *     false:
 *         No timeout occurred or parser was NULL.
 */
bool protocol_parser_advance_time_us(protocol_parser_t *parser, uint32_t elapsed_us)
{
    if ((parser == NULL) || (parser->state == PROTOCOL_PARSER_WAIT_SOF_0))
    {
        return false;
    }

    if (elapsed_us >= (PROTOCOL_PARTIAL_FRAME_TIMEOUT_US
                       - parser->partial_frame_elapsed_us))
    {
        protocol_increment_saturating(&parser->timeout_count);
        protocol_parser_reset_candidate(parser);
        return true;
    }

    parser->partial_frame_elapsed_us += elapsed_us;
    return false;
}
