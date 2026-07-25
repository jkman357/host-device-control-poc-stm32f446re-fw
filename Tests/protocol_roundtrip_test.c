// Copyright (c) 2026 Ray Yang. All rights reserved.
//
// File:
//     protocol_roundtrip_test.c
//
// Purpose:
//     Implements host-side protocol unit checks.
//
// Responsibilities:
//     - Verifies the published CRC check value.
//     - Verifies byte-level encode and parse round trips.
//     - Verifies CRC failure detection.
//     - Maps internal test results to process exit status explicitly.

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "protocol.h"
#include "protocol_crc.h"
#include "protocol_messages.h"

#define HOST_TEST_EXPECTED_CRC                   (0x29B1u)
#define HOST_TEST_SEQUENCE                       (0x1234u)
#define HOST_TEST_SEQUENCE_LOW_BYTE              (0x34u)
#define HOST_TEST_SEQUENCE_HIGH_BYTE             (0x12u)
#define HOST_TEST_MESSAGE_ID_LOW_BYTE             (0x00u)
#define HOST_TEST_MESSAGE_ID_HIGH_BYTE            (0x20u)
#define HOST_TEST_CRC_CORRUPTION_MASK             (0x01u)
#define HOST_TEST_CRC_ERROR_COUNT_EXPECTED        (1u)
#define HOST_TEST_SINGLE_SEQUENCE                 (1u)
#define HOST_TEST_FRAME_PAYLOAD_OFFSET            (9u)
#define HOST_TEST_FRAME_SOF_0_OFFSET              (0u)
#define HOST_TEST_FRAME_SOF_1_OFFSET              (1u)
#define HOST_TEST_FRAME_VERSION_OFFSET            (2u)
#define HOST_TEST_FRAME_MESSAGE_ID_LOW_OFFSET     (3u)
#define HOST_TEST_FRAME_MESSAGE_ID_HIGH_OFFSET    (4u)
#define HOST_TEST_FRAME_FLAGS_OFFSET              (5u)
#define HOST_TEST_FRAME_PAYLOAD_LENGTH_OFFSET     (6u)
#define HOST_TEST_FRAME_SEQUENCE_LOW_OFFSET       (7u)
#define HOST_TEST_FRAME_SEQUENCE_HIGH_OFFSET      (8u)
#define HOST_TEST_PAYLOAD_OFFSET_0                (0u)
#define HOST_TEST_PAYLOAD_OFFSET_1                (1u)
#define HOST_TEST_PAYLOAD_OFFSET_2                (2u)
#define HOST_TEST_PAYLOAD_OFFSET_3                (3u)
#define HOST_TEST_PAYLOAD_BYTE_0                  (0x11u)
#define HOST_TEST_PAYLOAD_BYTE_1                  (0x22u)
#define HOST_TEST_PAYLOAD_BYTE_2                  (0x33u)
#define HOST_TEST_PAYLOAD_BYTE_3                  (0x44u)
#define HOST_TEST_CORRUPTED_PAYLOAD_BYTE          (0x5Au)
#define HOST_TEST_CORRUPTED_PAYLOAD_LENGTH        (1u)
#define HOST_TEST_PASS_MESSAGE                    "protocol_roundtrip_test: PASS"
#define HOST_TEST_FAIL_MESSAGE                    "protocol_roundtrip_test: FAIL"

typedef enum
{
    HOST_TEST_RESULT_PASS = 0u,
    HOST_TEST_RESULT_CRC_CHECK_FAILED,
    HOST_TEST_RESULT_ENCODE_FAILED,
    HOST_TEST_RESULT_FRAME_LAYOUT_FAILED,
    HOST_TEST_RESULT_PARSE_FAILED,
    HOST_TEST_RESULT_DECODED_CONTENT_FAILED,
    HOST_TEST_RESULT_CRC_REJECTION_FAILED,
    HOST_TEST_RESULT_OUTPUT_FAILED
} host_test_result_t;

static const uint8_t s_crc_check_data[] =
{
    (uint8_t)'1',
    (uint8_t)'2',
    (uint8_t)'3',
    (uint8_t)'4',
    (uint8_t)'5',
    (uint8_t)'6',
    (uint8_t)'7',
    (uint8_t)'8',
    (uint8_t)'9'
};

static const uint8_t s_round_trip_payload[] =
{
    HOST_TEST_PAYLOAD_BYTE_0,
    HOST_TEST_PAYLOAD_BYTE_1,
    HOST_TEST_PAYLOAD_BYTE_2,
    HOST_TEST_PAYLOAD_BYTE_3
};

/*
 * Function:
 *     host_test_known_crc_value
 *
 * Purpose:
 *     Verifies the published CRC-16/CCITT-FALSE check value.
 *
 * Input Parameters:
 *     None.
 *
 * Output Parameters:
 *     None.
 *
 * Return Value:
 *     HOST_TEST_RESULT_PASS:
 *         The calculated CRC matched the published value.
 *     HOST_TEST_RESULT_CRC_CHECK_FAILED:
 *         The calculated CRC did not match the published value.
 */
static host_test_result_t host_test_known_crc_value(void)
{
    uint16_t crc;
    size_t index;

    crc = PROTOCOL_CRC_INITIAL_VALUE;
    index = 0u;
    while (index < sizeof(s_crc_check_data))
    {
        crc = protocol_crc_update(crc, s_crc_check_data[index]);
        index += 1u;
    }

    if (crc != HOST_TEST_EXPECTED_CRC)
    {
        return HOST_TEST_RESULT_CRC_CHECK_FAILED;
    }

    return HOST_TEST_RESULT_PASS;
}

/*
 * Function:
 *     host_test_round_trip
 *
 * Purpose:
 *     Verifies frame encoding, field placement, and byte-by-byte parsing.
 *
 * Input Parameters:
 *     None.
 *
 * Output Parameters:
 *     None.
 *
 * Return Value:
 *     HOST_TEST_RESULT_PASS:
 *         Encoding and parsing produced the expected frame content.
 *     HOST_TEST_RESULT_ENCODE_FAILED:
 *         Frame encoding failed.
 *     HOST_TEST_RESULT_FRAME_LAYOUT_FAILED:
 *         Encoded bytes did not match the expected layout.
 *     HOST_TEST_RESULT_PARSE_FAILED:
 *         The parser did not produce one complete frame at the final byte.
 *     HOST_TEST_RESULT_DECODED_CONTENT_FAILED:
 *         Decoded fields or payload bytes were incorrect.
 */
static host_test_result_t host_test_round_trip(void)
{
    protocol_parser_t parser;
    protocol_frame_t decoded_frame;
    uint8_t encoded_frame[PROTOCOL_MAX_FRAME_LENGTH];
    size_t encoded_length;
    size_t index;
    protocol_parse_result_t parse_result;
    bool is_encoded;

    protocol_parser_init(&parser);
    is_encoded = protocol_encode_frame(PROTOCOL_MESSAGE_TELEMETRY,
                                       PROTOCOL_FLAG_TELEMETRY,
                                       HOST_TEST_SEQUENCE,
                                       s_round_trip_payload,
                                       (uint8_t)sizeof(s_round_trip_payload),
                                       encoded_frame,
                                       sizeof(encoded_frame),
                                       &encoded_length);

    if (is_encoded == false)
    {
        return HOST_TEST_RESULT_ENCODE_FAILED;
    }

    if ((encoded_length != (PROTOCOL_FRAME_OVERHEAD_LENGTH + sizeof(s_round_trip_payload))) ||
        (encoded_frame[HOST_TEST_FRAME_SOF_0_OFFSET] != PROTOCOL_SOF_0) ||
        (encoded_frame[HOST_TEST_FRAME_SOF_1_OFFSET] != PROTOCOL_SOF_1) ||
        (encoded_frame[HOST_TEST_FRAME_VERSION_OFFSET] != PROTOCOL_VERSION) ||
        (encoded_frame[HOST_TEST_FRAME_MESSAGE_ID_LOW_OFFSET] != HOST_TEST_MESSAGE_ID_LOW_BYTE) ||
        (encoded_frame[HOST_TEST_FRAME_MESSAGE_ID_HIGH_OFFSET] != HOST_TEST_MESSAGE_ID_HIGH_BYTE) ||
        (encoded_frame[HOST_TEST_FRAME_FLAGS_OFFSET] != PROTOCOL_FLAG_TELEMETRY) ||
        (encoded_frame[HOST_TEST_FRAME_PAYLOAD_LENGTH_OFFSET] != sizeof(s_round_trip_payload)) ||
        (encoded_frame[HOST_TEST_FRAME_SEQUENCE_LOW_OFFSET] != HOST_TEST_SEQUENCE_LOW_BYTE) ||
        (encoded_frame[HOST_TEST_FRAME_SEQUENCE_HIGH_OFFSET] != HOST_TEST_SEQUENCE_HIGH_BYTE))
    {
        return HOST_TEST_RESULT_FRAME_LAYOUT_FAILED;
    }

    parse_result = PROTOCOL_PARSE_NO_FRAME;
    index = 0u;
    while (index < encoded_length)
    {
        parse_result = protocol_parser_push_byte(&parser, encoded_frame[index], &decoded_frame);

        if ((index < (encoded_length - 1u)) && (parse_result != PROTOCOL_PARSE_NO_FRAME))
        {
            return HOST_TEST_RESULT_PARSE_FAILED;
        }

        index += 1u;
    }

    if (parse_result != PROTOCOL_PARSE_FRAME_READY)
    {
        return HOST_TEST_RESULT_PARSE_FAILED;
    }

    if ((decoded_frame.message_id != PROTOCOL_MESSAGE_TELEMETRY) ||
        (decoded_frame.flags != PROTOCOL_FLAG_TELEMETRY) ||
        (decoded_frame.sequence != HOST_TEST_SEQUENCE) ||
        (decoded_frame.payload_length != sizeof(s_round_trip_payload)) ||
        (decoded_frame.payload[HOST_TEST_PAYLOAD_OFFSET_0] != HOST_TEST_PAYLOAD_BYTE_0) ||
        (decoded_frame.payload[HOST_TEST_PAYLOAD_OFFSET_1] != HOST_TEST_PAYLOAD_BYTE_1) ||
        (decoded_frame.payload[HOST_TEST_PAYLOAD_OFFSET_2] != HOST_TEST_PAYLOAD_BYTE_2) ||
        (decoded_frame.payload[HOST_TEST_PAYLOAD_OFFSET_3] != HOST_TEST_PAYLOAD_BYTE_3))
    {
        return HOST_TEST_RESULT_DECODED_CONTENT_FAILED;
    }

    return HOST_TEST_RESULT_PASS;
}

/*
 * Function:
 *     host_test_crc_failure
 *
 * Purpose:
 *     Verifies that a changed payload byte is rejected by CRC validation.
 *
 * Input Parameters:
 *     None.
 *
 * Output Parameters:
 *     None.
 *
 * Return Value:
 *     HOST_TEST_RESULT_PASS:
 *         The corrupted frame was rejected and the CRC diagnostic incremented once.
 *     HOST_TEST_RESULT_ENCODE_FAILED:
 *         Frame encoding failed.
 *     HOST_TEST_RESULT_CRC_REJECTION_FAILED:
 *         The corrupted frame was not rejected as expected.
 */
static host_test_result_t host_test_crc_failure(void)
{
    protocol_parser_t parser;
    protocol_frame_t decoded_frame;
    uint8_t encoded_frame[PROTOCOL_MAX_FRAME_LENGTH];
    uint8_t payload[HOST_TEST_CORRUPTED_PAYLOAD_LENGTH];
    size_t encoded_length;
    size_t index;
    protocol_parse_result_t parse_result;
    bool is_encoded;

    payload[HOST_TEST_PAYLOAD_OFFSET_0] = HOST_TEST_CORRUPTED_PAYLOAD_BYTE;
    is_encoded = protocol_encode_frame(PROTOCOL_MESSAGE_PING_RESPONSE,
                                       PROTOCOL_FLAG_RESPONSE,
                                       HOST_TEST_SINGLE_SEQUENCE,
                                       payload,
                                       (uint8_t)sizeof(payload),
                                       encoded_frame,
                                       sizeof(encoded_frame),
                                       &encoded_length);
    if (is_encoded == false)
    {
        return HOST_TEST_RESULT_ENCODE_FAILED;
    }

    encoded_frame[HOST_TEST_FRAME_PAYLOAD_OFFSET] ^= HOST_TEST_CRC_CORRUPTION_MASK;
    protocol_parser_init(&parser);
    parse_result = PROTOCOL_PARSE_NO_FRAME;
    index = 0u;
    while (index < encoded_length)
    {
        parse_result = protocol_parser_push_byte(&parser, encoded_frame[index], &decoded_frame);
        index += 1u;
    }

    if ((parse_result != PROTOCOL_PARSE_CRC_ERROR) ||
        (parser.crc_error_count != HOST_TEST_CRC_ERROR_COUNT_EXPECTED))
    {
        return HOST_TEST_RESULT_CRC_REJECTION_FAILED;
    }

    return HOST_TEST_RESULT_PASS;
}

/*
 * Function:
 *     host_test_to_exit_status
 *
 * Purpose:
 *     Maps an internal host-test result to a standard process exit status.
 *
 * Input Parameters:
 *     test_result:
 *         Supplies the internal host-test result.
 *
 * Output Parameters:
 *     None.
 *
 * Return Value:
 *     EXIT_SUCCESS:
 *         test_result is HOST_TEST_RESULT_PASS.
 *     EXIT_FAILURE:
 *         test_result reports any failure.
 */
static int host_test_to_exit_status(host_test_result_t test_result)
{
    if (test_result == HOST_TEST_RESULT_PASS)
    {
        return EXIT_SUCCESS;
    }

    return EXIT_FAILURE;
}

/*
 * Function:
 *     main
 *
 * Purpose:
 *     Executes protocol unit checks and reports one process result.
 *
 * Input Parameters:
 *     None.
 *
 * Output Parameters:
 *     None.
 *
 * Return Value:
 *     EXIT_SUCCESS:
 *         All checks passed and the result message was written.
 *     EXIT_FAILURE:
 *         A check or result-message write failed.
 *
 * Notes:
 *     The name main is required by the hosted C execution environment.
 */
int main(void)
{
    host_test_result_t test_result;
    int output_result;

    test_result = host_test_known_crc_value();
    if (test_result == HOST_TEST_RESULT_PASS)
    {
        test_result = host_test_round_trip();
    }
    if (test_result == HOST_TEST_RESULT_PASS)
    {
        test_result = host_test_crc_failure();
    }

    if (test_result == HOST_TEST_RESULT_PASS)
    {
        output_result = puts(HOST_TEST_PASS_MESSAGE);
    }
    else
    {
        output_result = puts(HOST_TEST_FAIL_MESSAGE);
    }

    if (output_result == EOF)
    {
        test_result = HOST_TEST_RESULT_OUTPUT_FAILED;
    }

    return host_test_to_exit_status(test_result);
}
