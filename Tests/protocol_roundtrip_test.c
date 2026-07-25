// Copyright (c) 2026 Ray Yang. All rights reserved.
//
// File:
//     protocol_roundtrip_test.c
//
// Purpose:
//     Verifies byte-level agreement with authoritative shared test vectors.
//
// Responsibilities:
//     - Checks CRC-16/CCITT-FALSE.
//     - Encodes every normative vector to exact bytes.
//     - Decodes every normative vector to declared fields.
//     - Verifies CRC rejection and partial-frame timeout behavior.

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "protocol.h"
#include "protocol_crc.h"
#include "protocol_messages.h"

#define HOST_TEST_PASS_MESSAGE              "protocol shared-vector test: PASS"
#define HOST_TEST_FAIL_MESSAGE              "protocol shared-vector test: FAIL"
#define HOST_TEST_CRC_EXPECTED               (0x29B1u)
#define HOST_TEST_CRC_CORRUPTION_MASK        (0x01u)
#define HOST_TEST_TIMEOUT_HALF_US            (125000u)
#define HOST_TEST_TELEMETRY_PAYLOAD_OFFSET    (8u)

static const uint8_t s_crc_check_input[] =
{
    '1', '2', '3', '4', '5', '6', '7', '8', '9'
};

static const uint8_t s_ping_frame[] =
{
    0xA5u, 0x5Au, 0x01u, 0x01u, 0x01u, 0x00u, 0x00u, 0x00u, 0x55u, 0x97u
};

static const uint8_t s_ack_ping_payload[] =
{
    0x01u, 0x00u, 0x00u
};

static const uint8_t s_ack_ping_frame[] =
{
    0xA5u, 0x5Au, 0x01u, 0x80u, 0x01u, 0x00u, 0x03u, 0x00u,
    0x01u, 0x00u, 0x00u, 0x53u, 0x6Fu
};

static const uint8_t s_set_config_payload[] =
{
    0x88u, 0x13u
};

static const uint8_t s_set_config_frame[] =
{
    0xA5u, 0x5Au, 0x01u, 0x03u, 0x34u, 0x12u, 0x02u, 0x00u,
    0x88u, 0x13u, 0x90u, 0x9Au
};

static const uint8_t s_start_stream_frame[] =
{
    0xA5u, 0x5Au, 0x01u, 0x04u, 0x02u, 0x00u, 0x00u, 0x00u, 0xDEu, 0x2Fu
};

static const uint8_t s_telemetry_payload[] =
{
    0x01u, 0x00u, 0x00u, 0x00u, 0x88u, 0x13u, 0x00u,
    0x00u, 0x91u, 0xA8u, 0x00u, 0x3Du, 0x00u, 0x00u
};

static const uint8_t s_telemetry_frame[] =
{
    0xA5u, 0x5Au, 0x01u, 0x90u, 0x01u, 0x00u, 0x0Eu, 0x00u,
    0x01u, 0x00u, 0x00u, 0x00u, 0x88u, 0x13u, 0x00u, 0x00u,
    0x91u, 0xA8u, 0x00u, 0x3Du, 0x00u, 0x00u, 0x8Du, 0xCFu
};

typedef enum
{
    HOST_TEST_RESULT_PASS = 0u,
    HOST_TEST_RESULT_CRC_FAILED,
    HOST_TEST_RESULT_ENCODE_FAILED,
    HOST_TEST_RESULT_ENCODE_BYTES_FAILED,
    HOST_TEST_RESULT_PARSE_FAILED,
    HOST_TEST_RESULT_DECODE_FAILED,
    HOST_TEST_RESULT_CRC_REJECTION_FAILED,
    HOST_TEST_RESULT_TIMEOUT_FAILED,
    HOST_TEST_RESULT_OUTPUT_FAILED
} host_test_result_t;

/*
 * Function:
 *     host_test_compare_bytes
 *
 * Purpose:
 *     Compares two byte sequences.
 *
 * Input Parameters:
 *     left:
 *         Points to the first byte sequence.
 *     right:
 *         Points to the second byte sequence.
 *     length:
 *         Supplies the number of bytes to compare.
 *
 * Output Parameters:
 *     None.
 *
 * Return Value:
 *     true:
 *         All bytes matched.
 *     false:
 *         A pointer was NULL or one byte differed.
 */
static bool host_test_compare_bytes(const uint8_t *left,
                                    const uint8_t *right,
                                    size_t length)
{
    size_t index;

    if ((left == NULL) || (right == NULL))
    {
        return false;
    }

    index = 0u;
    while (index < length)
    {
        if (left[index] != right[index])
        {
            return false;
        }
        index += 1u;
    }

    return true;
}

/*
 * Function:
 *     host_test_known_crc_value
 *
 * Purpose:
 *     Verifies the standard CRC check vector.
 *
 * Input Parameters:
 *     None.
 *
 * Output Parameters:
 *     None.
 *
 * Return Value:
 *     HOST_TEST_RESULT_PASS:
 *         CRC matched 0x29B1.
 *     HOST_TEST_RESULT_CRC_FAILED:
 *         CRC differed.
 */
static host_test_result_t host_test_known_crc_value(void)
{
    uint16_t crc;
    size_t index;

    crc = PROTOCOL_CRC_INITIAL_VALUE;
    index = 0u;
    while (index < sizeof(s_crc_check_input))
    {
        crc = protocol_crc_update(crc, s_crc_check_input[index]);
        index += 1u;
    }

    if (crc != HOST_TEST_CRC_EXPECTED)
    {
        return HOST_TEST_RESULT_CRC_FAILED;
    }

    return HOST_TEST_RESULT_PASS;
}

/*
 * Function:
 *     host_test_encode_vector
 *
 * Purpose:
 *     Encodes one normative vector and compares exact wire bytes.
 *
 * Input Parameters:
 *     message_id:
 *         Supplies the vector message identifier.
 *     sequence:
 *         Supplies the vector sequence.
 *     payload:
 *         Points to vector payload bytes or is NULL for an empty payload.
 *     payload_length:
 *         Supplies the payload length.
 *     expected_frame:
 *         Points to normative frame bytes.
 *     expected_length:
 *         Supplies the normative frame length.
 *
 * Output Parameters:
 *     None.
 *
 * Return Value:
 *     HOST_TEST_RESULT_PASS:
 *         Encoding matched exactly.
 *     HOST_TEST_RESULT_ENCODE_FAILED:
 *         Encoder rejected the vector.
 *     HOST_TEST_RESULT_ENCODE_BYTES_FAILED:
 *         Length or bytes differed.
 */
static host_test_result_t host_test_encode_vector(uint8_t message_id,
                                                  uint16_t sequence,
                                                  const uint8_t *payload,
                                                  uint16_t payload_length,
                                                  const uint8_t *expected_frame,
                                                  size_t expected_length)
{
    uint8_t encoded_frame[PROTOCOL_MAX_FRAME_LENGTH];
    size_t encoded_length;
    bool is_encoded;

    is_encoded = protocol_encode_frame(message_id,
                                       sequence,
                                       payload,
                                       payload_length,
                                       encoded_frame,
                                       sizeof(encoded_frame),
                                       &encoded_length);
    if (is_encoded == false)
    {
        return HOST_TEST_RESULT_ENCODE_FAILED;
    }

    if ((encoded_length != expected_length) ||
        (host_test_compare_bytes(encoded_frame, expected_frame, expected_length) == false))
    {
        return HOST_TEST_RESULT_ENCODE_BYTES_FAILED;
    }

    return HOST_TEST_RESULT_PASS;
}

/*
 * Function:
 *     host_test_decode_vector
 *
 * Purpose:
 *     Decodes one normative vector and checks declared fields and payload.
 *
 * Input Parameters:
 *     frame_bytes:
 *         Points to normative frame bytes.
 *     frame_length:
 *         Supplies the frame length.
 *     expected_message_id:
 *         Supplies the expected message identifier.
 *     expected_sequence:
 *         Supplies the expected sequence.
 *     expected_payload:
 *         Points to expected payload bytes or is NULL for an empty payload.
 *     expected_payload_length:
 *         Supplies the expected payload length.
 *
 * Output Parameters:
 *     None.
 *
 * Return Value:
 *     HOST_TEST_RESULT_PASS:
 *         Decoding matched all declared fields.
 *     HOST_TEST_RESULT_PARSE_FAILED:
 *         Parser did not complete exactly at the final byte.
 *     HOST_TEST_RESULT_DECODE_FAILED:
 *         A decoded field or payload differed.
 */
static host_test_result_t host_test_decode_vector(const uint8_t *frame_bytes,
                                                  size_t frame_length,
                                                  uint8_t expected_message_id,
                                                  uint16_t expected_sequence,
                                                  const uint8_t *expected_payload,
                                                  uint16_t expected_payload_length)
{
    protocol_parser_t parser;
    protocol_frame_t frame;
    protocol_parse_result_t parse_result;
    size_t index;

    protocol_parser_init(&parser);
    parse_result = PROTOCOL_PARSE_NO_FRAME;
    index = 0u;
    while (index < frame_length)
    {
        parse_result = protocol_parser_push_byte(&parser, frame_bytes[index], &frame);
        if ((index < (frame_length - 1u)) && (parse_result != PROTOCOL_PARSE_NO_FRAME))
        {
            return HOST_TEST_RESULT_PARSE_FAILED;
        }
        index += 1u;
    }

    if (parse_result != PROTOCOL_PARSE_FRAME_READY)
    {
        return HOST_TEST_RESULT_PARSE_FAILED;
    }

    if ((frame.version != PROTOCOL_VERSION) ||
        (frame.message_id != expected_message_id) ||
        (frame.sequence != expected_sequence) ||
        (frame.payload_length != expected_payload_length))
    {
        return HOST_TEST_RESULT_DECODE_FAILED;
    }

    if ((expected_payload_length > 0u) &&
        (host_test_compare_bytes(frame.payload,
                                 expected_payload,
                                 expected_payload_length) == false))
    {
        return HOST_TEST_RESULT_DECODE_FAILED;
    }

    return HOST_TEST_RESULT_PASS;
}

/*
 * Function:
 *     host_test_all_vectors
 *
 * Purpose:
 *     Encodes and decodes all normative shared vectors.
 *
 * Input Parameters:
 *     None.
 *
 * Output Parameters:
 *     None.
 *
 * Return Value:
 *     HOST_TEST_RESULT_PASS:
 *         Every vector passed.
 *     Other values:
 *         The first vector failure.
 */
static host_test_result_t host_test_all_vectors(void)
{
    host_test_result_t result;

    result = host_test_encode_vector(PROTOCOL_MESSAGE_PING,
                                     1u,
                                     NULL,
                                     0u,
                                     s_ping_frame,
                                     sizeof(s_ping_frame));
    if (result == HOST_TEST_RESULT_PASS)
    {
        result = host_test_decode_vector(s_ping_frame,
                                         sizeof(s_ping_frame),
                                         PROTOCOL_MESSAGE_PING,
                                         1u,
                                         NULL,
                                         0u);
    }
    if (result == HOST_TEST_RESULT_PASS)
    {
        result = host_test_encode_vector(PROTOCOL_MESSAGE_ACK,
                                         1u,
                                         s_ack_ping_payload,
                                         sizeof(s_ack_ping_payload),
                                         s_ack_ping_frame,
                                         sizeof(s_ack_ping_frame));
    }
    if (result == HOST_TEST_RESULT_PASS)
    {
        result = host_test_decode_vector(s_ack_ping_frame,
                                         sizeof(s_ack_ping_frame),
                                         PROTOCOL_MESSAGE_ACK,
                                         1u,
                                         s_ack_ping_payload,
                                         sizeof(s_ack_ping_payload));
    }
    if (result == HOST_TEST_RESULT_PASS)
    {
        result = host_test_encode_vector(PROTOCOL_MESSAGE_SET_STREAM_CONFIG,
                                         0x1234u,
                                         s_set_config_payload,
                                         sizeof(s_set_config_payload),
                                         s_set_config_frame,
                                         sizeof(s_set_config_frame));
    }
    if (result == HOST_TEST_RESULT_PASS)
    {
        result = host_test_decode_vector(s_set_config_frame,
                                         sizeof(s_set_config_frame),
                                         PROTOCOL_MESSAGE_SET_STREAM_CONFIG,
                                         0x1234u,
                                         s_set_config_payload,
                                         sizeof(s_set_config_payload));
    }
    if (result == HOST_TEST_RESULT_PASS)
    {
        result = host_test_encode_vector(PROTOCOL_MESSAGE_START_STREAM,
                                         2u,
                                         NULL,
                                         0u,
                                         s_start_stream_frame,
                                         sizeof(s_start_stream_frame));
    }
    if (result == HOST_TEST_RESULT_PASS)
    {
        result = host_test_decode_vector(s_start_stream_frame,
                                         sizeof(s_start_stream_frame),
                                         PROTOCOL_MESSAGE_START_STREAM,
                                         2u,
                                         NULL,
                                         0u);
    }
    if (result == HOST_TEST_RESULT_PASS)
    {
        result = host_test_encode_vector(PROTOCOL_MESSAGE_TELEMETRY_SAMPLE,
                                         1u,
                                         s_telemetry_payload,
                                         sizeof(s_telemetry_payload),
                                         s_telemetry_frame,
                                         sizeof(s_telemetry_frame));
    }
    if (result == HOST_TEST_RESULT_PASS)
    {
        result = host_test_decode_vector(s_telemetry_frame,
                                         sizeof(s_telemetry_frame),
                                         PROTOCOL_MESSAGE_TELEMETRY_SAMPLE,
                                         1u,
                                         s_telemetry_payload,
                                         sizeof(s_telemetry_payload));
    }

    return result;
}

/*
 * Function:
 *     host_test_crc_failure
 *
 * Purpose:
 *     Verifies controlled CRC corruption rejection.
 *
 * Input Parameters:
 *     None.
 *
 * Output Parameters:
 *     None.
 *
 * Return Value:
 *     HOST_TEST_RESULT_PASS:
 *         Corrupted frame was rejected and counted once.
 *     HOST_TEST_RESULT_CRC_REJECTION_FAILED:
 *         CRC rejection behavior differed.
 */
static host_test_result_t host_test_crc_failure(void)
{
    protocol_parser_t parser;
    protocol_frame_t frame;
    uint8_t corrupted_frame[sizeof(s_telemetry_frame)];
    protocol_parse_result_t parse_result;
    size_t index;

    index = 0u;
    while (index < sizeof(corrupted_frame))
    {
        corrupted_frame[index] = s_telemetry_frame[index];
        index += 1u;
    }
    corrupted_frame[HOST_TEST_TELEMETRY_PAYLOAD_OFFSET] ^= HOST_TEST_CRC_CORRUPTION_MASK;

    protocol_parser_init(&parser);
    parse_result = PROTOCOL_PARSE_NO_FRAME;
    index = 0u;
    while (index < sizeof(corrupted_frame))
    {
        parse_result = protocol_parser_push_byte(&parser, corrupted_frame[index], &frame);
        index += 1u;
    }

    if ((parse_result != PROTOCOL_PARSE_CRC_ERROR) || (parser.crc_error_count != 1u))
    {
        return HOST_TEST_RESULT_CRC_REJECTION_FAILED;
    }

    return HOST_TEST_RESULT_PASS;
}

/*
 * Function:
 *     host_test_partial_timeout
 *
 * Purpose:
 *     Verifies the authoritative 250-millisecond partial-frame timeout.
 *
 * Input Parameters:
 *     None.
 *
 * Output Parameters:
 *     None.
 *
 * Return Value:
 *     HOST_TEST_RESULT_PASS:
 *         Partial frame was retained then discarded at the timeout.
 *     HOST_TEST_RESULT_TIMEOUT_FAILED:
 *         Timeout behavior differed.
 */
static host_test_result_t host_test_partial_timeout(void)
{
    protocol_parser_t parser;
    protocol_frame_t frame;
    protocol_parse_result_t parse_result;
    bool did_timeout;

    protocol_parser_init(&parser);
    parse_result = protocol_parser_push_byte(&parser, PROTOCOL_SOF_0, &frame);
    if (parse_result != PROTOCOL_PARSE_NO_FRAME)
    {
        return HOST_TEST_RESULT_TIMEOUT_FAILED;
    }

    did_timeout = protocol_parser_advance_time_us(&parser, HOST_TEST_TIMEOUT_HALF_US);
    if (did_timeout == true)
    {
        return HOST_TEST_RESULT_TIMEOUT_FAILED;
    }

    did_timeout = protocol_parser_advance_time_us(&parser, HOST_TEST_TIMEOUT_HALF_US);
    if ((did_timeout == false) || (parser.timeout_count != 1u) ||
        (parser.state != PROTOCOL_PARSER_WAIT_SOF_0))
    {
        return HOST_TEST_RESULT_TIMEOUT_FAILED;
    }

    return HOST_TEST_RESULT_PASS;
}

/*
 * Function:
 *     host_test_to_exit_status
 *
 * Purpose:
 *     Maps an internal test result to a process exit status.
 *
 * Input Parameters:
 *     test_result:
 *         Supplies the internal result.
 *
 * Output Parameters:
 *     None.
 *
 * Return Value:
 *     EXIT_SUCCESS:
 *         test_result is PASS.
 *     EXIT_FAILURE:
 *         test_result reports a failure.
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
 *     Executes shared-Protocol unit checks and reports one process result.
 *
 * Input Parameters:
 *     None.
 *
 * Output Parameters:
 *     None.
 *
 * Return Value:
 *     EXIT_SUCCESS:
 *         All checks passed and output succeeded.
 *     EXIT_FAILURE:
 *         A check or output operation failed.
 */
int main(void)
{
    host_test_result_t test_result;
    int output_result;

    test_result = host_test_known_crc_value();
    if (test_result == HOST_TEST_RESULT_PASS)
    {
        test_result = host_test_all_vectors();
    }
    if (test_result == HOST_TEST_RESULT_PASS)
    {
        test_result = host_test_crc_failure();
    }
    if (test_result == HOST_TEST_RESULT_PASS)
    {
        test_result = host_test_partial_timeout();
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
