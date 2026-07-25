// Copyright (c) 2026 Ray Yang. All rights reserved.
//
// File:
//     app.h
//
// Purpose:
//     Defines the public contract for the application controller.
//
// Public Contract:
//     - Initializes application state and protocol processing.
//     - Processes coalesced events in the main execution context.
//     - Keeps application state private to app.c.

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
 *     Initialize application state and protocol processing.
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
 *     Processes one coherent event batch in the main execution context.
 *
 * Input Parameters:
 *     event_batch:
 *         Points to a caller-owned event batch. The function does not
 *         retain or modify the referenced object.
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
