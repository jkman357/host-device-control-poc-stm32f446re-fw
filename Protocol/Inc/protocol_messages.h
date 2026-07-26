// Copyright (c) 2026 Ray Yang. All rights reserved.
//
// File:
//     protocol_messages.h
//
// Purpose:
//     Defines protocol message identifiers, result codes, and stream limits.
//
// Public Contract:
//     - Provides named constants for the PoC wire contract.
//     - Defines finite result-code and status-flag domains.
//
// Notes:
//     These constants mirror the current PoC protocol snapshot.

#ifndef PROTOCOL_MESSAGES_H
#define PROTOCOL_MESSAGES_H

#include <stdint.h>

#define PROTOCOL_MESSAGE_PING (0x01u)
#define PROTOCOL_MESSAGE_GET_DEVICE_INFO (0x02u)
#define PROTOCOL_MESSAGE_SET_STREAM_CONFIG (0x03u)
#define PROTOCOL_MESSAGE_START_STREAM (0x04u)
#define PROTOCOL_MESSAGE_STOP_STREAM (0x05u)
#define PROTOCOL_MESSAGE_ACK (0x80u)
#define PROTOCOL_MESSAGE_NACK (0x81u)
#define PROTOCOL_MESSAGE_DEVICE_INFO (0x82u)
#define PROTOCOL_MESSAGE_DEVICE_STATUS (0x83u)
#define PROTOCOL_MESSAGE_TELEMETRY_SAMPLE (0x90u)
#define PROTOCOL_MESSAGE_ERROR_REPORT (0x91u)

#define PROTOCOL_STREAM_INTERVAL_MIN_US (1000u)
#define PROTOCOL_STREAM_INTERVAL_MAX_US (60000u)
#define PROTOCOL_STREAM_INTERVAL_DEFAULT_US (5000u)
#define PROTOCOL_DEVICE_TYPE_STM32F446RE (0x4460u)
#define PROTOCOL_DEVICE_NAME_MAX_LENGTH (32u)

typedef enum
{
    PROTOCOL_RESULT_OK = 0x00u,
    PROTOCOL_RESULT_INVALID_COMMAND = 0x01u,
    PROTOCOL_RESULT_INVALID_LENGTH = 0x02u,
    PROTOCOL_RESULT_INVALID_VALUE = 0x03u,
    PROTOCOL_RESULT_INVALID_STATE = 0x04u,
    PROTOCOL_RESULT_UNSUPPORTED_VERSION = 0x05u,
    PROTOCOL_RESULT_INTERNAL_ERROR = 0x06u
} protocol_result_code_t;

typedef enum
{
    PROTOCOL_STATUS_NONE = 0x0000u,
    PROTOCOL_STATUS_RX_OVERFLOW_OBSERVED = 0x0001u,
    PROTOCOL_STATUS_TX_OVERFLOW_OBSERVED = 0x0002u,
    PROTOCOL_STATUS_UART_ERROR_OBSERVED = 0x0004u
} protocol_status_flag_t;

#endif
