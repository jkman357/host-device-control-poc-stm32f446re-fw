// Copyright (c) 2026 Ray Yang. All rights reserved.
//
// File:
//     protocol_crc.h
//
// Purpose:
//     Defines the public CRC-16/CCITT-FALSE update contract.
//
// Public Contract:
//     - Exposes the initial CRC value and byte-update function.

#ifndef PROTOCOL_CRC_H
#define PROTOCOL_CRC_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PROTOCOL_CRC_INITIAL_VALUE      (0xFFFFu)

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
uint16_t protocol_crc_update(uint16_t crc, uint8_t data_byte);

#ifdef __cplusplus
}
#endif

#endif // PROTOCOL_CRC_H
