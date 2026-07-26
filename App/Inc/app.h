// Copyright (c) 2026 Ray Yang. All rights reserved.

#ifndef APP_H
#define APP_H

#include "app_event.h"

void app_init(void);
void app_process_event(const app_event_t *event);

#endif
