#ifndef DEVICE_STATE_H
#define DEVICE_STATE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    DEVICE_STATE_IDLE = 0u,
    DEVICE_STATE_STREAMING = 1u,
    DEVICE_STATE_FAULT = 2u
} device_state_t;

#ifdef __cplusplus
}
#endif

#endif /* DEVICE_STATE_H */
