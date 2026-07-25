#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "protocol.h"
#include "protocol_crc.h"
#include "protocol_messages.h"

/**
 * @brief Verify the published CRC-16/CCITT-FALSE check value.
 */
static void Test_KnownCrcValue(void)
{
    static const uint8_t check_data[] =
    {
        (uint8_t)'1', (uint8_t)'2', (uint8_t)'3',
        (uint8_t)'4', (uint8_t)'5', (uint8_t)'6',
        (uint8_t)'7', (uint8_t)'8', (uint8_t)'9'
    };
    uint16_t crc;
    size_t index;

    crc = PROTOCOL_CRC_INITIAL_VALUE;
    index = 0u;
    while (index < sizeof(check_data))
    {
        crc = ProtocolCrc_Update(crc, check_data[index]);
        index += 1u;
    }

    assert(crc == 0x29B1u);
}

/**
 * @brief Verify frame encoding, 16-bit fields, and byte-by-byte parsing.
 */
static void Test_RoundTrip(void)
{
    protocol_parser_t parser;
    protocol_frame_t decoded_frame;
    uint8_t encoded_frame[PROTOCOL_MAX_FRAME_LENGTH];
    const uint8_t payload[] = {0x11u, 0x22u, 0x33u, 0x44u};
    size_t encoded_length;
    size_t index;
    protocol_parse_result_t parse_result;
    bool encoded;

    ProtocolParser_Init(&parser);
    encoded = Protocol_EncodeFrame(PROTOCOL_MESSAGE_TELEMETRY,
                                   PROTOCOL_FLAG_TELEMETRY,
                                   0x1234u,
                                   payload,
                                   (uint8_t)sizeof(payload),
                                   encoded_frame,
                                   sizeof(encoded_frame),
                                   &encoded_length);

    assert(encoded == true);
    assert(encoded_length == (PROTOCOL_FRAME_OVERHEAD_LENGTH + sizeof(payload)));
    assert(encoded_frame[0] == PROTOCOL_SOF_0);
    assert(encoded_frame[1] == PROTOCOL_SOF_1);
    assert(encoded_frame[2] == PROTOCOL_VERSION);
    assert(encoded_frame[3] == 0x00u);
    assert(encoded_frame[4] == 0x20u);
    assert(encoded_frame[5] == PROTOCOL_FLAG_TELEMETRY);
    assert(encoded_frame[6] == sizeof(payload));
    assert(encoded_frame[7] == 0x34u);
    assert(encoded_frame[8] == 0x12u);

    parse_result = PROTOCOL_PARSE_NO_FRAME;
    index = 0u;
    while (index < encoded_length)
    {
        parse_result = ProtocolParser_PushByte(&parser, encoded_frame[index], &decoded_frame);
        if (index < (encoded_length - 1u))
        {
            assert(parse_result == PROTOCOL_PARSE_NO_FRAME);
        }
        index += 1u;
    }

    assert(parse_result == PROTOCOL_PARSE_FRAME_READY);
    assert(decoded_frame.message_id == PROTOCOL_MESSAGE_TELEMETRY);
    assert(decoded_frame.flags == PROTOCOL_FLAG_TELEMETRY);
    assert(decoded_frame.sequence == 0x1234u);
    assert(decoded_frame.payload_length == sizeof(payload));
    assert(decoded_frame.payload[0] == 0x11u);
    assert(decoded_frame.payload[1] == 0x22u);
    assert(decoded_frame.payload[2] == 0x33u);
    assert(decoded_frame.payload[3] == 0x44u);
}

/**
 * @brief Verify that a changed payload byte is detected by CRC.
 */
static void Test_CrcFailure(void)
{
    protocol_parser_t parser;
    protocol_frame_t decoded_frame;
    uint8_t encoded_frame[PROTOCOL_MAX_FRAME_LENGTH];
    const uint8_t payload[] = {0x5Au};
    size_t encoded_length;
    size_t index;
    protocol_parse_result_t parse_result;
    bool encoded;

    encoded = Protocol_EncodeFrame(PROTOCOL_MESSAGE_PING_RESPONSE,
                                   PROTOCOL_FLAG_RESPONSE,
                                   1u,
                                   payload,
                                   (uint8_t)sizeof(payload),
                                   encoded_frame,
                                   sizeof(encoded_frame),
                                   &encoded_length);
    assert(encoded == true);

    encoded_frame[9] ^= 0x01u;
    ProtocolParser_Init(&parser);
    parse_result = PROTOCOL_PARSE_NO_FRAME;
    index = 0u;
    while (index < encoded_length)
    {
        parse_result = ProtocolParser_PushByte(&parser, encoded_frame[index], &decoded_frame);
        index += 1u;
    }

    assert(parse_result == PROTOCOL_PARSE_CRC_ERROR);
    assert(parser.crc_error_count == 1u);
}

/**
 * @brief Execute protocol unit checks.
 * @return Zero when all checks pass.
 */
int main(void)
{
    Test_KnownCrcValue();
    Test_RoundTrip();
    Test_CrcFailure();

    puts("protocol_roundtrip_test: PASS");
    return 0;
}
