// Copyright (c) 2026 Ray Yang. All rights reserved.
//
// File:
//     app.h
//
// Purpose:
//     Defines the public application-control interface.
//
// Public Contract:
//     - Initializes the application state and protocol parser.
//     - Dispatches ordered application events without exposing private state.
//
// Notes:
//     Callers shall initialize the event, platform, and transport layers first.

#ifndef APP_H
#define APP_H

#include "app_event.h"

/*
 * Function:
 *     app_init
 *
 * Purpose:
 *     Initializes application-owned state, parser state, and the initial
 *     sample-timer interval.
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
 *     Call once from main context after platform and transport initialization.
 */
void app_init(void);
/*
 * Function:
 *     app_process_event
 *
 * Purpose:
 *     Validates and dispatches one ordered application event.
 *
 * Input Parameters:
 *     event:
 *         Pointer to the event to process. The referenced event is not modified.
 *
 * Output Parameters:
 *     None.
 *
 * Return Value:
 *     None.
 *
 * Notes:
 *     Runs in main context and owns all application state transitions.
 */
void app_process_event(const app_event_t *event);

#endif
