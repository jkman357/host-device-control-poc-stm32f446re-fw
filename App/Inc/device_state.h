// Copyright (c) 2026 Ray Yang. All rights reserved.
//
// File:
//     device_state.h
//
// Purpose:
//     Defines the externally reported device operating states.
//
// Public Contract:
//     - Provides the finite device-state domain used by the application and protocol responses.
//
// Notes:
//     State transitions remain owned by app.c.

#ifndef DEVICE_STATE_H
#define DEVICE_STATE_H

typedef enum
{
    DEVICE_STATE_IDLE = 0u,
    DEVICE_STATE_STREAMING = 1u
} device_state_t;

#endif
