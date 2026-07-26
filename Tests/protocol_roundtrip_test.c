// Copyright (c) 2026 Ray Yang. All rights reserved.

#include <assert.h>
#include <stddef.h>
#include <stdint.h>

#include "protocol.h"
#include "protocol_messages.h"

int main(void)
{
    uint8_t payload[14] = { 0u };
    uint8_t encoded[64];
    size_t encoded_length = 0u;
    protocol_parser_t parser;
    protocol_frame_t frame;
    protocol_parse_result_t result = PROTOCOL_PARSE_NO_FRAME;
    size_t index;

    protocol_write_u32_le(&payload[0], 1u);
    protocol_write_u32_le(&payload[4], 5000u);
    protocol_write_float32_le(&payload[8], 0.5f);
    protocol_write_u16_le(&payload[12], 0u);

    assert(protocol_encode_frame(
        PROTOCOL_MESSAGE_TELEMETRY_SAMPLE,
        1u,
        payload,
        (uint16_t)sizeof(payload),
        encoded,
        sizeof(encoded),
        &encoded_length));
    assert(encoded_length == 24u);

    protocol_parser_init(&parser);
    for (index = 0u; index < encoded_length; index += 1u)
    {
        result = protocol_parser_push_byte(&parser, encoded[index], &frame);
    }

    assert(result == PROTOCOL_PARSE_FRAME_READY);
    assert(frame.message_id == PROTOCOL_MESSAGE_TELEMETRY_SAMPLE);
    assert(frame.sequence == 1u);
    assert(frame.payload_length == 14u);
    assert(protocol_read_u32_le(frame.payload) == 1u);

    encoded[encoded_length - 1u] ^= 1u;
    protocol_parser_init(&parser);
    for (index = 0u; index < encoded_length; index += 1u)
    {
        result = protocol_parser_push_byte(&parser, encoded[index], &frame);
    }

    assert(result == PROTOCOL_PARSE_CRC_ERROR);

    return 0;
}
