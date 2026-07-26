// Copyright (c) 2026 Ray Yang. All rights reserved.

#include <assert.h>
#include <stddef.h>
#include <stdint.h>

#include "serial_baud.h"

#define TEST_TELEMETRY_FRAME_BYTES (24u)
#define TEST_PROTOCOL_MIN_INTERVAL_US (1000u)

typedef struct
{
    uint32_t rate;
    uint32_t brr;
    uint32_t error_ppm;
    uint32_t command_only;
    uint32_t minimum_interval_us;
} expected_profile_t;

static const expected_profile_t s_expected_profiles[] =
{
    { 1200u,   0x3415u, 25u,    1u, 0u },
    { 2400u,   0x1A0Bu, 50u,    1u, 0u },
    { 4800u,   0x0D05u, 100u,   1u, 0u },
    { 9600u,   0x0683u, 200u,   1u, 0u },
    { 19200u,  0x0341u, 400u,   0u, 15000u },
    { 38400u,  0x01A1u, 799u,   0u, 7507u },
    { 57600u,  0x0116u, 799u,   0u, 5005u },
    { 115200u, 0x008Bu, 799u,   0u, 2503u },
    { 230400u, 0x0045u, 6441u,  0u, 1250u },
    { 460800u, 0x0023u, 7937u,  0u, 1000u },
    { 921600u, 0x0011u, 21242u, 0u, 1000u }
};

#define TEST_LIST_ENTRY(rate_) rate_,
static const uint32_t s_supported_rates[] =
{
    SERIAL_BAUD_FOR_EACH_SUPPORTED(TEST_LIST_ENTRY)
};
#undef TEST_LIST_ENTRY

static uint32_t effective_minimum_interval_us(uint32_t rate)
{
    uint32_t calculated;

    if (SERIAL_BAUD_IS_COMMAND_ONLY(rate) != 0u)
    {
        return 0u;
    }

    calculated = SERIAL_BAUD_CALCULATED_MIN_INTERVAL_US(
        SERIAL_BAUD_PERIPHERAL_CLOCK_HZ,
        rate,
        TEST_TELEMETRY_FRAME_BYTES,
        SERIAL_BAUD_BITS_PER_BYTE,
        SERIAL_BAUD_STREAM_RESERVE_PERCENT);

    return (calculated > TEST_PROTOCOL_MIN_INTERVAL_US)
        ? calculated
        : TEST_PROTOCOL_MIN_INTERVAL_US;
}

int main(void)
{
    size_t index;

    assert((sizeof(s_supported_rates) / sizeof(s_supported_rates[0])) == 11u);
    assert((sizeof(s_expected_profiles) / sizeof(s_expected_profiles[0])) == 11u);

    for (index = 0u;
         index < (sizeof(s_expected_profiles) / sizeof(s_expected_profiles[0]));
         index += 1u)
    {
        const expected_profile_t *expected = &s_expected_profiles[index];

        assert(s_supported_rates[index] == expected->rate);
        assert(SERIAL_BAUD_IS_SUPPORTED(expected->rate) != 0u);
        assert(SERIAL_BAUD_BRR(SERIAL_BAUD_PERIPHERAL_CLOCK_HZ,
                               expected->rate) == expected->brr);
        assert(SERIAL_BAUD_ERROR_PPM(SERIAL_BAUD_PERIPHERAL_CLOCK_HZ,
                                     expected->rate) == expected->error_ppm);
        assert(SERIAL_BAUD_ERROR_PPM(SERIAL_BAUD_PERIPHERAL_CLOCK_HZ,
                                     expected->rate)
               <= SERIAL_BAUD_MAX_ERROR_PPM);
        assert(SERIAL_BAUD_IS_COMMAND_ONLY(expected->rate)
               == expected->command_only);
        assert(effective_minimum_interval_us(expected->rate)
               == expected->minimum_interval_us);
    }

    assert(SERIAL_BAUD_IS_SUPPORTED(14400u) == 0u);
    return 0;
}
