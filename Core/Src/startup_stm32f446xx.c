// Copyright (c) 2026 Ray Yang. All rights reserved.
//
// File:
//     startup_stm32f446xx.c
//
// Purpose:
//     Implements the minimal STM32F446 startup sequence and vector table.
//
// Responsibilities:
//     - Enables the Cortex-M4 floating-point unit before hard-float code executes.
//     - Initializes the data and zero-initialized memory sections.
//     - Transfers control to main and provides bounded default interrupt handling.
//
// Notes:
//     This is Product-owned startup code and does not depend on STM32Cube-generated sources.

#include <stdint.h>

typedef void (*startup_isr_callback_t)(void);

/*
 * Function:
 *     main
 *
 * Purpose:
 *     Declares the firmware entry point invoked after runtime initialization.
 *
 * Input Parameters:
 *     None.
 *
 * Output Parameters:
 *     None.
 *
 * Return Value:
 *     The entry point is not expected to return.
 */
extern int main(void);
/*
 * Function:
 *     USART2_IRQHandler
 *
 * Purpose:
 *     Declares the USART2 interrupt entry point installed in the vector table.
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
 *     Executes in interrupt context.
 */
extern void USART2_IRQHandler(void);
/*
 * Function:
 *     TIM6_DAC_IRQHandler
 *
 * Purpose:
 *     Declares the TIM6/DAC interrupt entry point installed in the vector table.
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
 *     Executes in interrupt context.
 */
extern void TIM6_DAC_IRQHandler(void);

extern uint32_t _estack;
extern uint32_t _sidata;
extern uint32_t _sdata;
extern uint32_t _edata;
extern uint32_t _sbss;
extern uint32_t _ebss;

#define SCB_CPACR (*(volatile uint32_t *)0xE000ED88u)
#define SCB_CPACR_CP10_FULL_ACCESS (3u << 20u)
#define SCB_CPACR_CP11_FULL_ACCESS (3u << 22u)

#define STARTUP_CORE_VECTOR_COUNT (16u)
#define STARTUP_EXTERNAL_VECTOR_COUNT (82u)
#define STARTUP_VECTOR_COUNT \
    (STARTUP_CORE_VECTOR_COUNT + STARTUP_EXTERNAL_VECTOR_COUNT)
#define STARTUP_DEFAULT_VECTOR_FIRST_INDEX (2u)
#define STARTUP_DEFAULT_VECTOR_LAST_INDEX (STARTUP_CORE_VECTOR_COUNT - 1u)
#define STARTUP_USART2_IRQ_NUMBER (38u)
#define STARTUP_TIM6_DAC_IRQ_NUMBER (54u)
#define STARTUP_USART2_VECTOR_INDEX \
    (STARTUP_CORE_VECTOR_COUNT + STARTUP_USART2_IRQ_NUMBER)
#define STARTUP_TIM6_DAC_VECTOR_INDEX \
    (STARTUP_CORE_VECTOR_COUNT + STARTUP_TIM6_DAC_IRQ_NUMBER)

/*
 * Function:
 *     startup_default_handler
 *
 * Purpose:
 *     Traps an unexpected interrupt in an intentional bounded idle loop.
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
 *     Executes in interrupt context and intentionally does not return.
 */
static void startup_default_handler(void)
{
    for (;;)
    {
    }
}

/*
 * Function:
 *     startup_enable_fpu
 *
 * Purpose:
 *     Enables full access to the Cortex-M4 CP10 and CP11 floating-point coprocessors
 *     and completes the required synchronization barriers.
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
 *     Called before any compiled hard-float instruction can execute.
 */
static void startup_enable_fpu(void)
{
    SCB_CPACR |=
        SCB_CPACR_CP10_FULL_ACCESS | SCB_CPACR_CP11_FULL_ACCESS;

    // Complete the CPACR write before any hard-float instruction can execute.
    __asm volatile("DSB" : : : "memory");
    __asm volatile("ISB" : : : "memory");
}

/*
 * Function:
 *     startup_reset_handler
 *
 * Purpose:
 *     Initializes the C runtime memory sections and transfers control to main.
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
 *     Executes immediately after reset and intentionally does not return.
 */
void startup_reset_handler(void)
{
    uint32_t *source;
    uint32_t *destination;

    source = &_sidata;
    destination = &_sdata;

    startup_enable_fpu();

    while (destination < &_edata)
    {
        *destination = *source;
        destination += 1;
        source += 1;
    }

    for (destination = &_sbss; destination < &_ebss; destination += 1)
    {
        *destination = 0u;
    }

    (void)main();

    for (;;)
    {
    }
}

// Global Object Record: GOR-CORE-001.
__attribute__((section(".isr_vector"), used))
const startup_isr_callback_t g_vector_table[STARTUP_VECTOR_COUNT] = {
    [0] = (startup_isr_callback_t)&_estack,
    [1] = startup_reset_handler,
    [STARTUP_DEFAULT_VECTOR_FIRST_INDEX ... STARTUP_DEFAULT_VECTOR_LAST_INDEX] =
        startup_default_handler,
    [STARTUP_USART2_VECTOR_INDEX] = USART2_IRQHandler,
    [STARTUP_TIM6_DAC_VECTOR_INDEX] = TIM6_DAC_IRQHandler,
};
