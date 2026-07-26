// Copyright (c) 2026 Ray Yang. All rights reserved.
//
// File:
//     protocol.h
//
// Purpose:
//     Defines the binary protocol codec and parser contract.
//
// Public Contract:
//     - Defines bounded frame and parser data structures.
//     - Encodes and incrementally parses protocol frames.
//     - Provides explicit little-endian serialization helpers.
//
// Notes:
//     Wire data is serialized field by field and never through packed-structure casts.

#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define PROTOCOL_SOF_0 (0xA5u)
#define PROTOCOL_SOF_1 (0x5Au)
#define PROTOCOL_VERSION (0x01u)
#define PROTOCOL_MAX_PAYLOAD_LENGTH (1024u)
#define PROTOCOL_FIXED_HEADER_LENGTH (6u)
#define PROTOCOL_FRAME_OVERHEAD_LENGTH (10u)
#define PROTOCOL_MAX_FRAME_LENGTH \
    (PROTOCOL_FRAME_OVERHEAD_LENGTH + PROTOCOL_MAX_PAYLOAD_LENGTH)
#define PROTOCOL_PARTIAL_FRAME_TIMEOUT_US (250000u)

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
void protocol_parser_init(protocol_parser_t *parser);
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
protocol_parse_result_t protocol_parser_push_byte(
    protocol_parser_t *parser,
    uint8_t data_byte,
    protocol_frame_t *frame);
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
bool protocol_parser_advance_time_us(
    protocol_parser_t *parser,
    uint32_t elapsed_us);
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
bool protocol_encode_frame(
    uint8_t message_id,
    uint16_t sequence,
    const uint8_t *payload,
    uint16_t payload_length,
    uint8_t *output,
    size_t output_capacity,
    size_t *encoded_length);
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
uint16_t protocol_crc16_ccitt_false(const uint8_t *data, size_t length);
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
uint16_t protocol_read_u16_le(const uint8_t *source);
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
uint32_t protocol_read_u32_le(const uint8_t *source);
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
void protocol_write_u16_le(uint8_t *destination, uint16_t value);
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
void protocol_write_u32_le(uint8_t *destination, uint32_t value);
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
void protocol_write_float32_le(uint8_t *destination, float value);

#endif
