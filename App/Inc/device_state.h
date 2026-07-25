// Copyright (c) 2026 Ray Yang. All rights reserved.
//
// File:
//     device_state.h
//
// Purpose:
//     Defines the authoritative externally visible device states.
//
// Public Contract:
//     - Mirrors the shared Protocol state_model values.
//     - Does not expose mutable application state.

#ifndef DEVICE_STATE_H
#define DEVICE_STATE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    DEVICE_STATE_IDLE = 0x00u,
    DEVICE_STATE_STREAMING = 0x01u
} device_state_t;

#ifdef __cplusplus
}
#endif

#endif // DEVICE_STATE_H
