#ifndef PROTOCOL_CRC_H
#define PROTOCOL_CRC_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PROTOCOL_CRC_INITIAL_VALUE      (0xFFFFu)

/**
 * @brief Update a CRC-16/CCITT-FALSE value with one byte.
 * @param crc Current CRC value.
 * @param data_byte Next byte.
 * @return Updated CRC value.
 */
uint16_t ProtocolCrc_Update(uint16_t crc, uint8_t data_byte);

#ifdef __cplusplus
}
#endif

#endif /* PROTOCOL_CRC_H */
