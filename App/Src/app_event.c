#include "app_event.h"

#include <limits.h>
#include <stddef.h>

#include "platform.h"

static volatile uint32_t s_event_flags;
static volatile uint16_t s_tick_count;
static volatile uint32_t s_tick_overflow_count;

/**
 * @brief Saturating increment for a 32-bit diagnostic counter.
 * @param counter Counter to increment.
 */
static void AppEvent_IncrementSaturatingU32(volatile uint32_t *counter)
{
    if (*counter < UINT32_MAX)
    {
        *counter += 1u;
    }
}

/**
 * @brief Initialize event state.
 */
void AppEvent_Init(void)
{
    s_event_flags = 0u;
    s_tick_count = 0u;
    s_tick_overflow_count = 0u;
}

/**
 * @brief Post one 5 ms tick from interrupt context.
 */
void AppEvent_PostTickFromIsr(void)
{
    if (s_tick_count < UINT16_MAX)
    {
        s_tick_count += 1u;
    }
    else
    {
        AppEvent_IncrementSaturatingU32(&s_tick_overflow_count);
    }
}

/**
 * @brief Post event flags from interrupt context.
 * @param flags Bitwise OR of APP_EVENT_FLAG values.
 */
void AppEvent_PostFlagsFromIsr(uint32_t flags)
{
    s_event_flags |= flags;
}

/**
 * @brief Atomically take all currently pending events.
 * @param[out] event_batch Destination batch.
 * @return True when at least one event was returned.
 */
bool AppEvent_Take(app_event_batch_t *event_batch)
{
    uint32_t primask;
    bool has_event;

    if (event_batch == NULL)
    {
        return false;
    }

    primask = Platform_IrqSave();

    event_batch->flags = s_event_flags;
    event_batch->tick_count = s_tick_count;
    event_batch->tick_overflow_count = s_tick_overflow_count;

    s_event_flags = 0u;
    s_tick_count = 0u;

    Platform_IrqRestore(primask);

    has_event = ((event_batch->flags != 0u) || (event_batch->tick_count != 0u));
    return has_event;
}

/**
 * @brief Atomically sleep until an interrupt can make an event pending.
 */
void AppEvent_Wait(void)
{
    uint32_t primask;

    primask = Platform_IrqSave();

    if ((s_event_flags == 0u) && (s_tick_count == 0u))
    {
        Platform_WaitForInterrupt();
    }

    Platform_IrqRestore(primask);
}
