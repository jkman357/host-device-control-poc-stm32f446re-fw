// Copyright (c) 2026 Ray Yang. All rights reserved.
//
// File:
//     protocol_crc.c
//
// Purpose:
//     Implements CRC-16/CCITT-FALSE byte processing.
//
// Responsibilities:
//     - Updates one CRC value for each supplied byte.
//     - Uses fixed-width arithmetic and bounded iteration.

#include "protocol_crc.h"

#define PROTOCOL_CRC_POLYNOMIAL     (0x1021u)
#define PROTOCOL_CRC_BIT_COUNT      (8u)
#define PROTOCOL_CRC_TOP_BIT        (0x8000u)

/*
 * Function:
 *     protocol_crc_update
 *
 * Purpose:
 *     Update a CRC-16/CCITT-FALSE value with one byte.
 *
 * Input Parameters:
 *     crc:
 *         Current CRC value.
 *     data_byte:
 *         Next byte.
 *
 * Output Parameters:
 *     None.
 *
 * Return Value:
 *     result:
 *         Updated CRC value.
 */
uint16_t protocol_crc_update(uint16_t crc, uint8_t data_byte)
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
