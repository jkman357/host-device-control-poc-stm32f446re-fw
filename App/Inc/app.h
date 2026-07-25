#ifndef APP_H
#define APP_H

#include "app_event.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize application state and protocol processing.
 */
void App_Init(void);

/**
 * @brief Process one batch of events in main context.
 * @param event_batch Event batch.
 */
void App_ProcessEvents(const app_event_batch_t *event_batch);

#ifdef __cplusplus
}
#endif

#endif /* APP_H */
