// Copyright (c) 2026 Ray Yang. All rights reserved.

#include "app.h"
#include "app_event.h"
#include "platform.h"
#include "serial_transport.h"

int main(void)
{
    app_event_t event;

    app_event_init();
    platform_init();
    serial_transport_init();
    app_init();

    for (;;)
    {
        if (app_event_take(&event))
        {
            app_process_event(&event);
        }
        else
        {
            app_event_wait();
        }
    }
}
