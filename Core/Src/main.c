#include "main.h"

#include "app.h"
#include "app_event.h"
#include "platform.h"
#include "serial_transport.h"

/**
 * @brief Initialize the platform and run the event-driven superloop.
 * @return This function does not return during normal operation.
 */
int main(void)
{
    app_event_batch_t event_batch;

    Platform_Init();
    AppEvent_Init();
    SerialTransport_Init();
    App_Init();
    Platform_StartFiveMillisecondTimer();

    for (;;)
    {
        if (AppEvent_Take(&event_batch) == true)
        {
            App_ProcessEvents(&event_batch);
        }
        else
        {
            AppEvent_Wait();
        }
    }
}
