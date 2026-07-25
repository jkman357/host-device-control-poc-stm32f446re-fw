// Copyright (c) 2026 Ray Yang. All rights reserved.
//
// File:
//     app.c
//
// Purpose:
//     Implements the temporary USART2 character-loopback application.
//
// Responsibilities:
//     - Drains received USART2 bytes in main context.
//     - Queues each received byte back to USART2 without modification.
//     - Toggles the board LED after each successfully queued echo byte.
//     - Records bounded diagnostic counters without blocking or retry loops.

#include "app.h"

#include <limits.h>
#include <stdbool.h>
#include <stdint.h>

#include "platform.h"
#include "serial_transport.h"

static uint32_t s_echoed_byte_count;
static uint32_t s_echo_failure_count;
static uint32_t s_uart_error_event_count;
static bool s_is_led_on;

/*
 * Function:
 *     app_increment_saturating_u32
 *
 * Purpose:
 *     Increments a diagnostic counter without unsigned wraparound.
 *
 * Input Parameters:
 *     counter:
 *         Points to the module-owned counter to update.
 *
 * Output Parameters:
 *     counter:
 *         Receives the incremented value or remains UINT32_MAX.
 *
 * Return Value:
 *     None.
 */
static void app_increment_saturating_u32(uint32_t *counter)
{
    if (*counter < UINT32_MAX)
    {
        *counter += 1u;
    }
}

/*
 * Function:
 *     app_echo_received_bytes
 *
 * Purpose:
 *     Drains all currently buffered RX bytes and queues each byte for echo.
 *
 * Input Parameters:
 *     None.
 *
 * Output Parameters:
 *     None.
 *
 * Return Value:
 *     None.
 *
 * Notes:
 *     Runs only in main context. The transport copies each byte before the
 *         function advances to the next received byte.
 */
static void app_echo_received_bytes(void)
{
    uint8_t data_byte;
    serial_transport_result_t transport_result;

    while (serial_transport_read_byte(&data_byte) == true)
    {
        transport_result = serial_transport_write(&data_byte, sizeof(data_byte));
        if (transport_result == SERIAL_TRANSPORT_RESULT_OK)
        {
            app_increment_saturating_u32(&s_echoed_byte_count);
            s_is_led_on = (s_is_led_on == false);
            platform_led_set(s_is_led_on);
        }
        else
        {
            app_increment_saturating_u32(&s_echo_failure_count);
        }
    }
}

/*
 * Function:
 *     app_init
 *
 * Purpose:
 *     Initializes the temporary character-loopback application state.
 *
 * Input Parameters:
 *     None.
 *
 * Output Parameters:
 *     None.
 *
 * Return Value:
 *     None.
 */
void app_init(void)
{
    s_echoed_byte_count = 0u;
    s_echo_failure_count = 0u;
    s_uart_error_event_count = 0u;
    s_is_led_on = false;
    platform_led_set(false);
}

/*
 * Function:
 *     app_process_events
 *
 * Purpose:
 *     Processes one event batch for the temporary USART2 loopback test.
 *
 * Input Parameters:
 *     event_batch:
 *         Points to a caller-owned event batch. The function does not retain
 *         or modify the referenced object.
 *
 * Output Parameters:
 *     None.
 *
 * Return Value:
 *     None.
 *
 * Notes:
 *     A NULL pointer is ignored. Protocol parsing and sample-timer handling
 *         are intentionally bypassed in this test firmware.
 */
void app_process_events(const app_event_batch_t *event_batch)
{
    if (event_batch == NULL)
    {
        return;
    }

    if ((event_batch->flags & APP_EVENT_FLAG_UART_ERROR) != 0u)
    {
        app_increment_saturating_u32(&s_uart_error_event_count);
    }

    if ((event_batch->flags & APP_EVENT_FLAG_UART_RX_AVAILABLE) != 0u)
    {
        app_echo_received_bytes();
    }
}
