// Copyright (c) 2026 Ray Yang. All rights reserved.
//
// File:
//     startup_stm32f446xx.c
//
// Purpose:
//     Implements STM32F446 startup and interrupt-vector integration.
//
// Responsibilities:
//     - Defines the interrupt vector table.
//     - Initializes data and BSS memory.
//     - Relocates the vector table and enters main.
//     - Provides bounded default exception behavior.

#include "startup_stm32f446xx.h"

#include <stdint.h>

#include "main.h"
#include "platform.h"
#include "serial_transport.h"

extern uint32_t _estack;
extern uint32_t _sidata;
extern uint32_t _sdata;
extern uint32_t _edata;
extern uint32_t _sbss;
extern uint32_t _ebss;

#define CORE_VECTOR_COUNT        (16u)
#define STM32_IRQ_COUNT          (97u)
#define TOTAL_VECTOR_COUNT       (CORE_VECTOR_COUNT + STM32_IRQ_COUNT)
#define SCB_VTOR_ADDRESS         (0xE000ED08u)
#define SCB_VTOR                 (*(volatile uint32_t *)SCB_VTOR_ADDRESS)

typedef void (*startup_interrupt_callback_t)(void);

typedef union
{
    uintptr_t initial_stack_pointer;
    startup_interrupt_callback_t interrupt_callback;
} startup_vector_entry_t;

_Static_assert(sizeof(startup_vector_entry_t) == sizeof(uint32_t),
               "startup vector entry size mismatch");

/*
 * Function:
 *     startup_default_handler
 *
 * Purpose:
 *     Stops execution for an unimplemented exception or interrupt.
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
 *     Execution Context: exception or ISR. The function intentionally does not return.
 */
void startup_default_handler(void)
{
    for (;;)
    {
        __asm volatile ("nop");
    }
}


/*
 * Function:
 *     startup_nmi_handler
 *
 * Purpose:
 *     Handles the Non-Maskable Interrupt when no specialized handler is installed.
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
 *     Execution Context: exception. This weak symbol aliases startup_default_handler unless overridden.
 */
void startup_nmi_handler(void) __attribute__((weak, alias("startup_default_handler")));

/*
 * Function:
 *     startup_hard_fault_handler
 *
 * Purpose:
 *     Handles the HardFault when no specialized handler is installed.
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
 *     Execution Context: exception. This weak symbol aliases startup_default_handler unless overridden.
 */
void startup_hard_fault_handler(void) __attribute__((weak, alias("startup_default_handler")));

/*
 * Function:
 *     startup_memory_management_fault_handler
 *
 * Purpose:
 *     Handles the memory-management fault when no specialized handler is installed.
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
 *     Execution Context: exception. This weak symbol aliases startup_default_handler unless overridden.
 */
void startup_memory_management_fault_handler(void) __attribute__((weak, alias("startup_default_handler")));

/*
 * Function:
 *     startup_bus_fault_handler
 *
 * Purpose:
 *     Handles the bus fault when no specialized handler is installed.
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
 *     Execution Context: exception. This weak symbol aliases startup_default_handler unless overridden.
 */
void startup_bus_fault_handler(void) __attribute__((weak, alias("startup_default_handler")));

/*
 * Function:
 *     startup_usage_fault_handler
 *
 * Purpose:
 *     Handles the usage fault when no specialized handler is installed.
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
 *     Execution Context: exception. This weak symbol aliases startup_default_handler unless overridden.
 */
void startup_usage_fault_handler(void) __attribute__((weak, alias("startup_default_handler")));

/*
 * Function:
 *     startup_supervisor_call_handler
 *
 * Purpose:
 *     Handles the supervisor call when no specialized handler is installed.
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
 *     Execution Context: exception. This weak symbol aliases startup_default_handler unless overridden.
 */
void startup_supervisor_call_handler(void) __attribute__((weak, alias("startup_default_handler")));

/*
 * Function:
 *     startup_debug_monitor_handler
 *
 * Purpose:
 *     Handles the debug monitor exception when no specialized handler is installed.
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
 *     Execution Context: exception. This weak symbol aliases startup_default_handler unless overridden.
 */
void startup_debug_monitor_handler(void) __attribute__((weak, alias("startup_default_handler")));

/*
 * Function:
 *     startup_pend_sv_handler
 *
 * Purpose:
 *     Handles the PendSV when no specialized handler is installed.
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
 *     Execution Context: exception. This weak symbol aliases startup_default_handler unless overridden.
 */
void startup_pend_sv_handler(void) __attribute__((weak, alias("startup_default_handler")));

/*
 * Function:
 *     startup_system_tick_handler
 *
 * Purpose:
 *     Handles the SysTick when no specialized handler is installed.
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
 *     Execution Context: exception. This weak symbol aliases startup_default_handler unless overridden.
 */
void startup_system_tick_handler(void) __attribute__((weak, alias("startup_default_handler")));

// Global Object Record: GOR-STARTUP-001.
__attribute__((section(".isr_vector"), used))
const startup_vector_entry_t g_startup_vector_table[TOTAL_VECTOR_COUNT] =
{
    { .initial_stack_pointer = (uintptr_t)&_estack }, // Vector 0: Initial stack pointer
    { .interrupt_callback = startup_reset_handler }, // Vector 1: Reset
    { .interrupt_callback = startup_nmi_handler }, // Vector 2: NMI
    { .interrupt_callback = startup_hard_fault_handler }, // Vector 3: HardFault
    { .interrupt_callback = startup_memory_management_fault_handler }, // Vector 4: MemManage
    { .interrupt_callback = startup_bus_fault_handler }, // Vector 5: BusFault
    { .interrupt_callback = startup_usage_fault_handler }, // Vector 6: UsageFault
    { .interrupt_callback = startup_default_handler }, // Vector 7: Reserved
    { .interrupt_callback = startup_default_handler }, // Vector 8: Reserved
    { .interrupt_callback = startup_default_handler }, // Vector 9: Reserved
    { .interrupt_callback = startup_default_handler }, // Vector 10: Reserved
    { .interrupt_callback = startup_supervisor_call_handler }, // Vector 11: SVCall
    { .interrupt_callback = startup_debug_monitor_handler }, // Vector 12: Debug monitor
    { .interrupt_callback = startup_default_handler }, // Vector 13: Reserved
    { .interrupt_callback = startup_pend_sv_handler }, // Vector 14: PendSV
    { .interrupt_callback = startup_system_tick_handler }, // Vector 15: SysTick
    { .interrupt_callback = startup_default_handler }, // IRQ 0: default
    { .interrupt_callback = startup_default_handler }, // IRQ 1: default
    { .interrupt_callback = startup_default_handler }, // IRQ 2: default
    { .interrupt_callback = startup_default_handler }, // IRQ 3: default
    { .interrupt_callback = startup_default_handler }, // IRQ 4: default
    { .interrupt_callback = startup_default_handler }, // IRQ 5: default
    { .interrupt_callback = startup_default_handler }, // IRQ 6: default
    { .interrupt_callback = startup_default_handler }, // IRQ 7: default
    { .interrupt_callback = startup_default_handler }, // IRQ 8: default
    { .interrupt_callback = startup_default_handler }, // IRQ 9: default
    { .interrupt_callback = startup_default_handler }, // IRQ 10: default
    { .interrupt_callback = startup_default_handler }, // IRQ 11: default
    { .interrupt_callback = startup_default_handler }, // IRQ 12: default
    { .interrupt_callback = startup_default_handler }, // IRQ 13: default
    { .interrupt_callback = startup_default_handler }, // IRQ 14: default
    { .interrupt_callback = startup_default_handler }, // IRQ 15: default
    { .interrupt_callback = startup_default_handler }, // IRQ 16: default
    { .interrupt_callback = startup_default_handler }, // IRQ 17: default
    { .interrupt_callback = startup_default_handler }, // IRQ 18: default
    { .interrupt_callback = startup_default_handler }, // IRQ 19: default
    { .interrupt_callback = startup_default_handler }, // IRQ 20: default
    { .interrupt_callback = startup_default_handler }, // IRQ 21: default
    { .interrupt_callback = startup_default_handler }, // IRQ 22: default
    { .interrupt_callback = startup_default_handler }, // IRQ 23: default
    { .interrupt_callback = startup_default_handler }, // IRQ 24: default
    { .interrupt_callback = startup_default_handler }, // IRQ 25: default
    { .interrupt_callback = startup_default_handler }, // IRQ 26: default
    { .interrupt_callback = startup_default_handler }, // IRQ 27: default
    { .interrupt_callback = startup_default_handler }, // IRQ 28: default
    { .interrupt_callback = startup_default_handler }, // IRQ 29: default
    { .interrupt_callback = startup_default_handler }, // IRQ 30: default
    { .interrupt_callback = startup_default_handler }, // IRQ 31: default
    { .interrupt_callback = startup_default_handler }, // IRQ 32: default
    { .interrupt_callback = startup_default_handler }, // IRQ 33: default
    { .interrupt_callback = startup_default_handler }, // IRQ 34: default
    { .interrupt_callback = startup_default_handler }, // IRQ 35: default
    { .interrupt_callback = startup_default_handler }, // IRQ 36: default
    { .interrupt_callback = startup_default_handler }, // IRQ 37: default
    { .interrupt_callback = serial_transport_usart2_irq_handler }, // IRQ 38: USART2
    { .interrupt_callback = startup_default_handler }, // IRQ 39: default
    { .interrupt_callback = startup_default_handler }, // IRQ 40: default
    { .interrupt_callback = startup_default_handler }, // IRQ 41: default
    { .interrupt_callback = startup_default_handler }, // IRQ 42: default
    { .interrupt_callback = startup_default_handler }, // IRQ 43: default
    { .interrupt_callback = startup_default_handler }, // IRQ 44: default
    { .interrupt_callback = startup_default_handler }, // IRQ 45: default
    { .interrupt_callback = startup_default_handler }, // IRQ 46: default
    { .interrupt_callback = startup_default_handler }, // IRQ 47: default
    { .interrupt_callback = startup_default_handler }, // IRQ 48: default
    { .interrupt_callback = startup_default_handler }, // IRQ 49: default
    { .interrupt_callback = startup_default_handler }, // IRQ 50: default
    { .interrupt_callback = startup_default_handler }, // IRQ 51: default
    { .interrupt_callback = startup_default_handler }, // IRQ 52: default
    { .interrupt_callback = startup_default_handler }, // IRQ 53: default
    { .interrupt_callback = platform_tim6_dac_irq_handler }, // IRQ 54: TIM6_DAC
    { .interrupt_callback = startup_default_handler }, // IRQ 55: default
    { .interrupt_callback = startup_default_handler }, // IRQ 56: default
    { .interrupt_callback = startup_default_handler }, // IRQ 57: default
    { .interrupt_callback = startup_default_handler }, // IRQ 58: default
    { .interrupt_callback = startup_default_handler }, // IRQ 59: default
    { .interrupt_callback = startup_default_handler }, // IRQ 60: default
    { .interrupt_callback = startup_default_handler }, // IRQ 61: default
    { .interrupt_callback = startup_default_handler }, // IRQ 62: default
    { .interrupt_callback = startup_default_handler }, // IRQ 63: default
    { .interrupt_callback = startup_default_handler }, // IRQ 64: default
    { .interrupt_callback = startup_default_handler }, // IRQ 65: default
    { .interrupt_callback = startup_default_handler }, // IRQ 66: default
    { .interrupt_callback = startup_default_handler }, // IRQ 67: default
    { .interrupt_callback = startup_default_handler }, // IRQ 68: default
    { .interrupt_callback = startup_default_handler }, // IRQ 69: default
    { .interrupt_callback = startup_default_handler }, // IRQ 70: default
    { .interrupt_callback = startup_default_handler }, // IRQ 71: default
    { .interrupt_callback = startup_default_handler }, // IRQ 72: default
    { .interrupt_callback = startup_default_handler }, // IRQ 73: default
    { .interrupt_callback = startup_default_handler }, // IRQ 74: default
    { .interrupt_callback = startup_default_handler }, // IRQ 75: default
    { .interrupt_callback = startup_default_handler }, // IRQ 76: default
    { .interrupt_callback = startup_default_handler }, // IRQ 77: default
    { .interrupt_callback = startup_default_handler }, // IRQ 78: default
    { .interrupt_callback = startup_default_handler }, // IRQ 79: default
    { .interrupt_callback = startup_default_handler }, // IRQ 80: default
    { .interrupt_callback = startup_default_handler }, // IRQ 81: default
    { .interrupt_callback = startup_default_handler }, // IRQ 82: default
    { .interrupt_callback = startup_default_handler }, // IRQ 83: default
    { .interrupt_callback = startup_default_handler }, // IRQ 84: default
    { .interrupt_callback = startup_default_handler }, // IRQ 85: default
    { .interrupt_callback = startup_default_handler }, // IRQ 86: default
    { .interrupt_callback = startup_default_handler }, // IRQ 87: default
    { .interrupt_callback = startup_default_handler }, // IRQ 88: default
    { .interrupt_callback = startup_default_handler }, // IRQ 89: default
    { .interrupt_callback = startup_default_handler }, // IRQ 90: default
    { .interrupt_callback = startup_default_handler }, // IRQ 91: default
    { .interrupt_callback = startup_default_handler }, // IRQ 92: default
    { .interrupt_callback = startup_default_handler }, // IRQ 93: default
    { .interrupt_callback = startup_default_handler }, // IRQ 94: default
    { .interrupt_callback = startup_default_handler }, // IRQ 95: default
    { .interrupt_callback = startup_default_handler } // IRQ 96: default
};

_Static_assert((sizeof(g_startup_vector_table) / sizeof(g_startup_vector_table[0])) == TOTAL_VECTOR_COUNT,
               "vector table size mismatch");

/*
 * Function:
 *     startup_reset_handler
 *
 * Purpose:
 *     Initializes runtime memory and transfers control to main.
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
 *     Execution Context: processor reset. The function does not return during normal operation.
 */
void startup_reset_handler(void)
{
    uint32_t *source;
    uint32_t *destination;

    SCB_VTOR = (uint32_t)(uintptr_t)&g_startup_vector_table[0];
    __asm volatile ("dsb" ::: "memory");
    __asm volatile ("isb" ::: "memory");

    source = &_sidata;
    destination = &_sdata;

    while (destination < &_edata)
    {
        *destination = *source;
        destination += 1;
        source += 1;
    }

    destination = &_sbss;
    while (destination < &_ebss)
    {
        *destination = 0u;
        destination += 1;
    }

    // main is specified not to return; an unexpected return enters the startup fail-stop loop.
    (void)main();

    for (;;)
    {
        __asm volatile ("nop");
    }
}
