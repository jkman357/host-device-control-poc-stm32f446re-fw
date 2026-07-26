// Copyright (c) 2026 Ray Yang. All rights reserved.

#ifndef APP_EVENT_H
#define APP_EVENT_H

#include <stdbool.h>
#include <stdint.h>

#define APP_EVENT_QUEUE_CAPACITY (256u)

typedef enum
{
    APP_EVENT_TYPE_UART_RX_BYTE = 0u,
    APP_EVENT_TYPE_SAMPLE_TICK = 1u,
    APP_EVENT_TYPE_UART_ERROR = 2u
} app_event_type_t;

typedef struct
{
    app_event_type_t type;
    uint8_t data_byte;
} app_event_t;

void app_event_init(void);
bool app_event_post_rx_byte_from_isr(uint8_t data_byte);
bool app_event_post_tick_from_isr(void);
bool app_event_post_uart_error_from_isr(void);
bool app_event_take(app_event_t *event);
uint32_t app_event_get_overflow_count(void);
void app_event_wait(void);

#endif
