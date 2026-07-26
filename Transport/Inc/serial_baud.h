// Copyright (c) 2026 Ray Yang. All rights reserved.

#ifndef SERIAL_BAUD_H
#define SERIAL_BAUD_H

#include <stdint.h>

#define SERIAL_BAUD_RATE_1200 (1200u)
#define SERIAL_BAUD_RATE_2400 (2400u)
#define SERIAL_BAUD_RATE_4800 (4800u)
#define SERIAL_BAUD_RATE_9600 (9600u)
#define SERIAL_BAUD_RATE_19200 (19200u)
#define SERIAL_BAUD_RATE_38400 (38400u)
#define SERIAL_BAUD_RATE_57600 (57600u)
#define SERIAL_BAUD_RATE_115200 (115200u)
#define SERIAL_BAUD_RATE_230400 (230400u)
#define SERIAL_BAUD_RATE_460800 (460800u)
#define SERIAL_BAUD_RATE_921600 (921600u)

#define SERIAL_BAUD_FOR_EACH_SUPPORTED(APPLY) \
    APPLY(SERIAL_BAUD_RATE_1200)              \
    APPLY(SERIAL_BAUD_RATE_2400)              \
    APPLY(SERIAL_BAUD_RATE_4800)              \
    APPLY(SERIAL_BAUD_RATE_9600)              \
    APPLY(SERIAL_BAUD_RATE_19200)             \
    APPLY(SERIAL_BAUD_RATE_38400)             \
    APPLY(SERIAL_BAUD_RATE_57600)             \
    APPLY(SERIAL_BAUD_RATE_115200)            \
    APPLY(SERIAL_BAUD_RATE_230400)            \
    APPLY(SERIAL_BAUD_RATE_460800)            \
    APPLY(SERIAL_BAUD_RATE_921600)

#ifndef SERIAL_TRANSPORT_BAUD_RATE
#define SERIAL_TRANSPORT_BAUD_RATE SERIAL_BAUD_RATE_460800
#endif

#define SERIAL_BAUD_PERIPHERAL_CLOCK_HZ (16000000u)
#define SERIAL_BAUD_BITS_PER_BYTE (10u)
#define SERIAL_BAUD_COMMAND_ONLY_MAX_RATE SERIAL_BAUD_RATE_9600
#define SERIAL_BAUD_MAX_ERROR_PPM (25000u)
#define SERIAL_BAUD_STREAM_RESERVE_PERCENT (20u)

#define SERIAL_BAUD_IS_SUPPORTED(rate_)                                  \
    ((((rate_) == SERIAL_BAUD_RATE_1200)                                 \
      || ((rate_) == SERIAL_BAUD_RATE_2400)                              \
      || ((rate_) == SERIAL_BAUD_RATE_4800)                              \
      || ((rate_) == SERIAL_BAUD_RATE_9600)                              \
      || ((rate_) == SERIAL_BAUD_RATE_19200)                             \
      || ((rate_) == SERIAL_BAUD_RATE_38400)                             \
      || ((rate_) == SERIAL_BAUD_RATE_57600)                             \
      || ((rate_) == SERIAL_BAUD_RATE_115200)                            \
      || ((rate_) == SERIAL_BAUD_RATE_230400)                            \
      || ((rate_) == SERIAL_BAUD_RATE_460800)                            \
      || ((rate_) == SERIAL_BAUD_RATE_921600))                           \
         ? 1u                                                            \
         : 0u)

#define SERIAL_BAUD_IS_COMMAND_ONLY(rate_) \
    (((rate_) <= SERIAL_BAUD_COMMAND_ONLY_MAX_RATE) ? 1u : 0u)

#define SERIAL_BAUD_BRR(clock_hz_, rate_) \
    (((clock_hz_) + ((rate_) / 2u)) / (rate_))

#define SERIAL_BAUD_ACTUAL_RATE(clock_hz_, rate_) \
    ((clock_hz_) / SERIAL_BAUD_BRR((clock_hz_), (rate_)))

#define SERIAL_BAUD_ABSOLUTE_DIFFERENCE(left_, right_) \
    (((left_) >= (right_)) ? ((left_) - (right_)) : ((right_) - (left_)))

#define SERIAL_BAUD_ERROR_PPM(clock_hz_, rate_)                              \
    ((uint32_t)((((uint64_t)SERIAL_BAUD_ABSOLUTE_DIFFERENCE(                 \
                       (clock_hz_),                                           \
                       ((rate_) * SERIAL_BAUD_BRR((clock_hz_), (rate_))))     \
                   * UINT64_C(1000000))                                       \
                  + ((uint64_t)(rate_)                                        \
                     * SERIAL_BAUD_BRR((clock_hz_), (rate_)) / 2u))           \
                 / ((uint64_t)(rate_)                                         \
                    * SERIAL_BAUD_BRR((clock_hz_), (rate_)))))

#define SERIAL_BAUD_SAFE_RATE(clock_hz_, rate_)                           \
    ((SERIAL_BAUD_ACTUAL_RATE((clock_hz_), (rate_)) < (rate_))            \
         ? SERIAL_BAUD_ACTUAL_RATE((clock_hz_), (rate_))                  \
         : (rate_))

#define SERIAL_BAUD_CEILING_DIV_U64(numerator_, denominator_) \
    (((numerator_) + (denominator_) - UINT64_C(1)) / (denominator_))

#define SERIAL_BAUD_CALCULATED_MIN_INTERVAL_US(                            \
    clock_hz_, rate_, frame_bytes_, bits_per_byte_, reserve_percent_)      \
    ((uint32_t)SERIAL_BAUD_CEILING_DIV_U64(                                \
        (UINT64_C(1000000) * (uint64_t)(frame_bytes_)                      \
         * (uint64_t)(bits_per_byte_)                                      \
         * (uint64_t)(100u + (reserve_percent_))),                         \
        ((uint64_t)SERIAL_BAUD_SAFE_RATE((clock_hz_), (rate_))             \
         * UINT64_C(100))))

#endif
