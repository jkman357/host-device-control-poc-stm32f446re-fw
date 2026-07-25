#include "protocol_crc.h"

#define PROTOCOL_CRC_POLYNOMIAL     (0x1021u)
#define PROTOCOL_CRC_BIT_COUNT      (8u)
#define PROTOCOL_CRC_TOP_BIT        (0x8000u)

/**
 * @brief Update a CRC-16/CCITT-FALSE value with one byte.
 * @param crc Current CRC value.
 * @param data_byte Next byte.
 * @return Updated CRC value.
 */
uint16_t ProtocolCrc_Update(uint16_t crc, uint8_t data_byte)
{
    uint8_t bit_index;

    crc ^= (uint16_t)((uint16_t)data_byte << 8u);
    bit_index = 0u;

    while (bit_index < PROTOCOL_CRC_BIT_COUNT)
    {
        if ((crc & PROTOCOL_CRC_TOP_BIT) != 0u)
        {
            crc = (uint16_t)((uint16_t)(crc << 1u) ^ PROTOCOL_CRC_POLYNOMIAL);
        }
        else
        {
            crc = (uint16_t)(crc << 1u);
        }

        bit_index += 1u;
    }

    return crc;
}
