// Copyright (c) 2026 Ray Yang. All rights reserved.
//
// File:
//     app.h
//
// Purpose:
//     Defines the temporary USART2 character-loopback application contract.
//
// Public Contract:
//     - Initializes loopback diagnostics and LED state.
//     - Processes coalesced UART events in main context.
//     - Echoes each received USART2 byte without protocol interpretation.

#ifndef APP_H
#define APP_H

#include "app_event.h"

#ifdef __cplusplus
extern "C" {
#endif

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
void app_init(void);

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
 *     A NULL pointer is ignored. The function shall not be called from
 *         interrupt context.
 */
void app_process_events(const app_event_batch_t *event_batch);

#ifdef __cplusplus
}
#endif

#endif // APP_H
