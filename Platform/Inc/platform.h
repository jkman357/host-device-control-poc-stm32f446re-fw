#ifndef PLATFORM_H
#define PLATFORM_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PLATFORM_CORE_CLOCK_HZ       (16000000u)
#define PLATFORM_TICK_PERIOD_MS      (5u)

/**
 * @brief Initialize the HSI clock, GPIO, and board LED.
 */
void Platform_Init(void);

/**
 * @brief Configure and start TIM6 as a 5 ms application event source.
 */
void Platform_StartFiveMillisecondTimer(void);

/**
 * @brief Set the NUCLEO LD2 LED state.
 * @param is_on True turns the LED on; false turns it off.
 */
void Platform_LedSet(bool is_on);

/**
 * @brief Wait for an interrupt while preserving the caller's interrupt-mask state.
 * @note Call with interrupts masked only after atomically checking event state.
 */
void Platform_WaitForInterrupt(void);

/**
 * @brief Disable interrupts and return the previous PRIMASK value.
 * @return Previous PRIMASK value.
 */
uint32_t Platform_IrqSave(void);

/**
 * @brief Restore a previously saved PRIMASK value.
 * @param primask Value returned by Platform_IrqSave.
 */
void Platform_IrqRestore(uint32_t primask);

#ifdef __cplusplus
}
#endif

#endif /* PLATFORM_H */
