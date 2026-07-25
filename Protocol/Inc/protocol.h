// Copyright (c) 2026 Ray Yang. All rights reserved.
//
// File:
//     protocol.h
//
// Purpose:
//     Defines the public shared-wire framing, parser, and serialization contract.
//
// Public Contract:
//     - Implements the authoritative 0.1.0 frame layout.
//     - Parses received bytes without dynamic allocation.
//     - Encodes frames into caller-owned buffers.
//     - Serializes little-endian integer and IEEE-754 float fields.

#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PROTOCOL_SOF_0                       (0xA5u)
#define PROTOCOL_SOF_1                       (0x5Au)
#define PROTOCOL_VERSION                     (0x01u)
#define PROTOCOL_MAX_PAYLOAD_LENGTH          (1024u)
#define PROTOCOL_FIXED_HEADER_LENGTH         (6u)
#define PROTOCOL_FRAME_OVERHEAD_LENGTH       (10u)
#define PROTOCOL_MAX_FRAME_LENGTH            (PROTOCOL_FRAME_OVERHEAD_LENGTH + PROTOCOL_MAX_PAYLOAD_LENGTH)
#define PROTOCOL_PARTIAL_FRAME_TIMEOUT_US    (250000u)

typedef struct
{
    uint8_t version;
    uint8_t message_id;
    uint16_t sequence;
    uint16_t payload_length;
    uint8_t payload[PROTOCOL_MAX_PAYLOAD_LENGTH];
} protocol_frame_t;

typedef enum
{
    PROTOCOL_PARSE_NO_FRAME = 0u,
    PROTOCOL_PARSE_FRAME_READY = 1u,
    PROTOCOL_PARSE_FORMAT_ERROR = 2u,
    PROTOCOL_PARSE_CRC_ERROR = 3u
} protocol_parse_result_t;

typedef enum
{
    PROTOCOL_PARSER_WAIT_SOF_0 = 0u,
    PROTOCOL_PARSER_WAIT_SOF_1,
    PROTOCOL_PARSER_READ_HEADER,
    PROTOCOL_PARSER_READ_PAYLOAD,
    PROTOCOL_PARSER_READ_CRC_LOW,
    PROTOCOL_PARSER_READ_CRC_HIGH
} protocol_parser_state_t;

typedef struct
{
    protocol_parser_state_t state;
    uint8_t header[PROTOCOL_FIXED_HEADER_LENGTH];
    uint8_t header_index;
    uint16_t payload_index;
    uint16_t calculated_crc;
    uint16_t received_crc;
    uint32_t partial_frame_elapsed_us;
    uint32_t format_error_count;
    uint32_t crc_error_count;
    uint32_t timeout_count;
} protocol_parser_t;

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
 *         Receives initialized parser state when the pointer is valid.
 *
 * Return Value:
 *     None.
 */
void protocol_parser_init(protocol_parser_t *parser);

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
 *     frame:
 *         Supplies caller-owned frame storage used while parsing.
 *
 * Output Parameters:
 *     parser:
 *         Receives updated parser state and diagnostic counters.
 *     frame:
 *         Receives a complete frame only when PROTOCOL_PARSE_FRAME_READY is returned.
 *
 * Return Value:
 *     PROTOCOL_PARSE_NO_FRAME:
 *         No complete frame is available.
 *     PROTOCOL_PARSE_FRAME_READY:
 *         A complete CRC-valid frame is available.
 *     PROTOCOL_PARSE_FORMAT_ERROR:
 *         An argument or payload length was invalid.
 *     PROTOCOL_PARSE_CRC_ERROR:
 *         The candidate frame CRC did not match.
 */
protocol_parse_result_t protocol_parser_push_byte(protocol_parser_t *parser,
                                                  uint8_t data_byte,
                                                  protocol_frame_t *frame);

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
bool protocol_parser_advance_time_us(protocol_parser_t *parser, uint32_t elapsed_us);

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
 *         Points to payload bytes, or is NULL for an empty payload.
 *     payload_length:
 *         Supplies the payload byte count.
 *     output:
 *         Points to caller-owned output storage.
 *     output_capacity:
 *         Supplies the output buffer capacity.
 *     encoded_length:
 *         Points to storage for the encoded byte count.
 *
 * Output Parameters:
 *     output:
 *         Receives the encoded frame only when true is returned.
 *     encoded_length:
 *         Receives the encoded byte count only when true is returned.
 *
 * Return Value:
 *     true:
 *         The frame was encoded successfully.
 *     false:
 *         An argument, length, or capacity was invalid.
 */
bool protocol_encode_frame(uint8_t message_id,
                           uint16_t sequence,
                           const uint8_t *payload,
                           uint16_t payload_length,
                           uint8_t *output,
                           size_t output_capacity,
                           size_t *encoded_length);

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
uint16_t protocol_read_u16_le(const uint8_t *source);

/*
 * Function:
 *     protocol_write_u16_le
 *
 * Purpose:
 *     Writes one 16-bit value in little-endian byte order.
 *
 * Input Parameters:
 *     destination:
 *         Points to at least two writable bytes.
 *     value:
 *         Supplies the value to serialize.
 *
 * Output Parameters:
 *     destination:
 *         Receives two serialized bytes when the pointer is valid.
 *
 * Return Value:
 *     None.
 */
void protocol_write_u16_le(uint8_t *destination, uint16_t value);

/*
 * Function:
 *     protocol_write_u32_le
 *
 * Purpose:
 *     Writes one 32-bit value in little-endian byte order.
 *
 * Input Parameters:
 *     destination:
 *         Points to at least four writable bytes.
 *     value:
 *         Supplies the value to serialize.
 *
 * Output Parameters:
 *     destination:
 *         Receives four serialized bytes when the pointer is valid.
 *
 * Return Value:
 *     None.
 */
void protocol_write_u32_le(uint8_t *destination, uint32_t value);

/*
 * Function:
 *     protocol_write_float32_le
 *
 * Purpose:
 *     Writes one IEEE-754 binary32 value in little-endian byte order.
 *
 * Input Parameters:
 *     destination:
 *         Points to at least four writable bytes.
 *     value:
 *         Supplies the float value to serialize.
 *
 * Output Parameters:
 *     destination:
 *         Receives four serialized bytes when the pointer is valid.
 *
 * Return Value:
 *     None.
 *
 * Notes:
 *     Supported MCU and host-test toolchains are little-endian IEEE-754 binary32.
 */
void protocol_write_float32_le(uint8_t *destination, float value);

#ifdef __cplusplus
}
#endif

#endif // PROTOCOL_H
