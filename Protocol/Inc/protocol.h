// Copyright (c) 2026 Ray Yang. All rights reserved.
//
// File:
//     protocol.h
//
// Purpose:
//     Defines the public framing, parser, and serialization contract.
//
// Public Contract:
//     - Defines bounded frame and parser types.
//     - Parses received bytes without dynamic allocation.
//     - Encodes frames into caller-owned buffers.
//     - Serializes little-endian integer fields.

#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PROTOCOL_SOF_0                   (0xA5u)
#define PROTOCOL_SOF_1                   (0x5Au)
#define PROTOCOL_VERSION                 (0x01u)
#define PROTOCOL_MAX_PAYLOAD_LENGTH      (48u)
#define PROTOCOL_FIXED_HEADER_LENGTH     (7u)
#define PROTOCOL_FRAME_OVERHEAD_LENGTH   (11u)
#define PROTOCOL_MAX_FRAME_LENGTH        (PROTOCOL_FRAME_OVERHEAD_LENGTH + PROTOCOL_MAX_PAYLOAD_LENGTH)

typedef struct
{
    uint16_t message_id;
    uint8_t flags;
    uint8_t payload_length;
    uint16_t sequence;
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
    uint8_t payload_index;
    uint16_t calculated_crc;
    uint16_t received_crc;
    uint32_t format_error_count;
    uint32_t crc_error_count;
} protocol_parser_t;

/*
 * Function:
 *     protocol_parser_init
 *
 * Purpose:
 *     Initializes a caller-owned protocol parser and clears its
 *         diagnostics.
 *
 * Input Parameters:
 *     None.
 *
 * Output Parameters:
 *     parser:
 *         Receives initialized parser state when the pointer is valid. A
 *         NULL pointer is ignored.
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
 *     Consumes one received byte and advances the caller-owned frame
 *         parser.
 *
 * Input Parameters:
 *     parser:
 *         Supplies the current parser state and is updated by the
 *         function.
 *     data_byte:
 *         Supplies the next received wire byte.
 *     frame:
 *         Supplies caller-owned frame storage used while parsing.
 *
 * Output Parameters:
 *     parser:
 *         Receives updated parser state and diagnostic counters.
 *     frame:
 *         Receives a completed frame only when PROTOCOL_PARSE_FRAME_READY
 *         is returned. Otherwise partial frame content is not a valid
 *         message.
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
                                                protocol_frame_t *frame);

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
 *         Points to payload bytes, or is NULL when payload_length is
 *         zero.
 *     payload_length:
 *         Supplies the number of payload bytes.
 *     output:
 *         Points to caller-owned output storage.
 *     output_capacity:
 *         Supplies the output buffer capacity.
 *     encoded_length:
 *         Points to storage for the encoded byte count.
 *
 * Output Parameters:
 *     output:
 *         Receives the complete encoded frame only when true is returned.
 *         The content is unspecified on failure.
 *     encoded_length:
 *         Receives the encoded byte count only when true is returned. The
 *         value remains unchanged on failure.
 *
 * Return Value:
 *     true:
 *         The frame was encoded successfully.
 *     false:
 *         An argument, payload length, or output capacity was invalid.
 *
 * Notes:
 *     The function does not retain caller-owned pointers.
 */
bool protocol_encode_frame(uint16_t message_id,
                          uint8_t flags,
                          uint16_t sequence,
                          const uint8_t *payload,
                          uint8_t payload_length,
                          uint8_t *output,
                          size_t output_capacity,
                          size_t *encoded_length);

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
 *         Receives two serialized bytes when the pointer is valid. A NULL
 *         pointer is ignored.
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
 *         Receives four serialized bytes when the pointer is valid. A
 *         NULL pointer is ignored.
 *
 * Return Value:
 *     None.
 */
void protocol_write_u32_le(uint8_t *destination, uint32_t value);

#ifdef __cplusplus
}
#endif

#endif // PROTOCOL_H
