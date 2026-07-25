#ifndef APP_EVENT_H
#define APP_EVENT_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define APP_EVENT_FLAG_UART_RX_AVAILABLE    (1u << 0u)
#define APP_EVENT_FLAG_UART_ERROR           (1u << 1u)

/**
 * @brief Batch of coalesced events transferred from interrupt context to main context.
 */
typedef struct
{
    uint32_t flags;
    uint16_t tick_count;
    uint32_t tick_overflow_count;
} app_event_batch_t;

/**
 * @brief Initialize event state.
 */
void AppEvent_Init(void);

/**
 * @brief Post one 5 ms tick from interrupt context.
 */
void AppEvent_PostTickFromIsr(void);

/**
 * @brief Post event flags from interrupt context.
 * @param flags Bitwise OR of APP_EVENT_FLAG values.
 */
void AppEvent_PostFlagsFromIsr(uint32_t flags);

/**
 * @brief Atomically take all currently pending events.
 * @param[out] event_batch Destination batch.
 * @return True when at least one event was returned.
 */
bool AppEvent_Take(app_event_batch_t *event_batch);

/**
 * @brief Atomically sleep until an interrupt can make an event pending.
 */
void AppEvent_Wait(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_EVENT_H */
