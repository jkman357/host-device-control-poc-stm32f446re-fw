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

/**
 * @brief Initialize a protocol parser instance.
 * @param parser Parser instance.
 */
void ProtocolParser_Init(protocol_parser_t *parser);

/**
 * @brief Push one received byte into the frame parser.
 * @param parser Parser instance.
 * @param data_byte Next received byte.
 * @param[out] frame Completed frame when the return value is FRAME_READY.
 * @return Parser result.
 */
protocol_parse_result_t ProtocolParser_PushByte(protocol_parser_t *parser,
                                                uint8_t data_byte,
                                                protocol_frame_t *frame);

/**
 * @brief Encode one frame into a caller-owned output buffer.
 * @param message_id Message identifier.
 * @param flags Frame flags.
 * @param sequence Sequence value.
 * @param payload Payload pointer, or NULL when payload_length is zero.
 * @param payload_length Payload length.
 * @param[out] output Output buffer.
 * @param output_capacity Output buffer capacity.
 * @param[out] encoded_length Number of encoded bytes.
 * @return True on success.
 */
bool Protocol_EncodeFrame(uint16_t message_id,
                          uint8_t flags,
                          uint16_t sequence,
                          const uint8_t *payload,
                          uint8_t payload_length,
                          uint8_t *output,
                          size_t output_capacity,
                          size_t *encoded_length);

/**
 * @brief Write a little-endian 16-bit value.
 * @param destination Two-byte destination.
 * @param value Value to write.
 */
void Protocol_WriteU16Le(uint8_t *destination, uint16_t value);

/**
 * @brief Write a little-endian 32-bit value.
 * @param destination Four-byte destination.
 * @param value Value to write.
 */
void Protocol_WriteU32Le(uint8_t *destination, uint32_t value);

#ifdef __cplusplus
}
#endif

#endif /* PROTOCOL_H */
