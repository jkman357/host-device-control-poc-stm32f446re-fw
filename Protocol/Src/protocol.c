// Copyright (c) 2026 Ray Yang. All rights reserved.
#include "protocol.h"

#include <limits.h>

#define CRC_INITIAL                         (0xFFFFu)
#define CRC_POLYNOMIAL                      (0x1021u)
#define HEADER_VERSION_OFFSET               (0u)
#define HEADER_MESSAGE_ID_OFFSET            (1u)
#define HEADER_SEQUENCE_OFFSET              (2u)
#define HEADER_PAYLOAD_LENGTH_OFFSET        (4u)

static void protocol_increment_saturating(uint32_t *value)
{
    if (*value < UINT32_MAX)
    {
        *value += 1u;
    }
}

static uint16_t protocol_crc_update(uint16_t crc, uint8_t data_byte)
{
    uint8_t bit_index;

    crc ^= (uint16_t)((uint16_t)data_byte << 8u);
    for (bit_index = 0u; bit_index < 8u; ++bit_index)
    {
        if ((crc & 0x8000u) != 0u)
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

static void protocol_parser_reset_candidate(protocol_parser_t *parser)
{
    parser->state = PROTOCOL_PARSER_WAIT_SOF_0;
    parser->header_index = 0u;
    parser->payload_index = 0u;
    parser->calculated_crc = CRC_INITIAL;
    parser->received_crc = 0u;
    parser->partial_frame_elapsed_us = 0u;
}

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

uint16_t protocol_crc16_ccitt_false(const uint8_t *data, size_t length)
{
    uint16_t crc = CRC_INITIAL;
    size_t data_index;

    if ((data == NULL) && (length != 0u))
    {
        return 0u;
    }

    for (data_index = 0u; data_index < length; ++data_index)
    {
        crc = protocol_crc_update(crc, data[data_index]);
    }

    return crc;
}

uint16_t protocol_read_u16_le(const uint8_t *source)
{
    if (source == NULL)
    {
        return 0u;
    }

    return (uint16_t)((uint16_t)source[0] | ((uint16_t)source[1] << 8u));
}

uint32_t protocol_read_u32_le(const uint8_t *source)
{
    if (source == NULL)
    {
        return 0u;
    }

    return (uint32_t)source[0]
         | ((uint32_t)source[1] << 8u)
         | ((uint32_t)source[2] << 16u)
         | ((uint32_t)source[3] << 24u);
}

void protocol_write_u16_le(uint8_t *destination, uint16_t value)
{
    if (destination != NULL)
    {
        destination[0] = (uint8_t)value;
        destination[1] = (uint8_t)(value >> 8u);
    }
}

void protocol_write_u32_le(uint8_t *destination, uint32_t value)
{
    if (destination != NULL)
    {
        destination[0] = (uint8_t)value;
        destination[1] = (uint8_t)(value >> 8u);
        destination[2] = (uint8_t)(value >> 16u);
        destination[3] = (uint8_t)(value >> 24u);
    }
}

void protocol_write_float32_le(uint8_t *destination, float value)
{
    union
    {
        float float_value;
        uint32_t uint32_value;
    } converter;

    converter.float_value = value;
    protocol_write_u32_le(destination, converter.uint32_value);
}

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

    output[0] = PROTOCOL_SOF_0;
    output[1] = PROTOCOL_SOF_1;
    output[2] = PROTOCOL_VERSION;
    output[3] = message_id;
    protocol_write_u16_le(&output[4], sequence);
    protocol_write_u16_le(&output[6], payload_length);

    for (payload_index = 0u; payload_index < payload_length; ++payload_index)
    {
        output[8u + payload_index] = payload[payload_index];
    }

    crc = protocol_crc16_ccitt_false(&output[2], (size_t)6u + payload_length);
    protocol_write_u16_le(&output[8u + payload_length], crc);
    *encoded_length = total_length;
    return true;
}

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
                frame->sequence = protocol_read_u16_le(&parser->header[HEADER_SEQUENCE_OFFSET]);
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
            parser->received_crc |= (uint16_t)((uint16_t)data_byte << 8u);
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
