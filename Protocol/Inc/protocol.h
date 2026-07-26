// Copyright (c) 2026 Ray Yang. All rights reserved.

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

void protocol_parser_init(protocol_parser_t *parser);
protocol_parse_result_t protocol_parser_push_byte(
    protocol_parser_t *parser,
    uint8_t data_byte,
    protocol_frame_t *frame);
bool protocol_parser_advance_time_us(
    protocol_parser_t *parser,
    uint32_t elapsed_us);
bool protocol_encode_frame(
    uint8_t message_id,
    uint16_t sequence,
    const uint8_t *payload,
    uint16_t payload_length,
    uint8_t *output,
    size_t output_capacity,
    size_t *encoded_length);
uint16_t protocol_crc16_ccitt_false(const uint8_t *data, size_t length);
uint16_t protocol_read_u16_le(const uint8_t *source);
uint32_t protocol_read_u32_le(const uint8_t *source);
void protocol_write_u16_le(uint8_t *destination, uint16_t value);
void protocol_write_u32_le(uint8_t *destination, uint32_t value);
void protocol_write_float32_le(uint8_t *destination, float value);

#endif
