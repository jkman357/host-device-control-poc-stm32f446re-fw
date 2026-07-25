#include <stdint.h>

#include "main.h"

extern uint32_t _estack;
extern uint32_t _sidata;
extern uint32_t _sdata;
extern uint32_t _edata;
extern uint32_t _sbss;
extern uint32_t _ebss;

void Reset_Handler(void);
void Default_Handler(void);
void USART2_IRQHandler(void);
void TIM6_DAC_IRQHandler(void);

#define CORE_VECTOR_COUNT        (16u)
#define STM32_IRQ_COUNT          (97u)
#define TOTAL_VECTOR_COUNT       (CORE_VECTOR_COUNT + STM32_IRQ_COUNT)
#define SCB_VTOR_ADDRESS         (0xE000ED08u)
#define SCB_VTOR                 (*(volatile uint32_t *)SCB_VTOR_ADDRESS)

/**
 * @brief Default exception and interrupt handler.
 */
void Default_Handler(void)
{
    for (;;)
    {
        __asm volatile ("nop");
    }
}

void NMI_Handler(void) __attribute__((weak, alias("Default_Handler")));
void HardFault_Handler(void) __attribute__((weak, alias("Default_Handler")));
void MemManage_Handler(void) __attribute__((weak, alias("Default_Handler")));
void BusFault_Handler(void) __attribute__((weak, alias("Default_Handler")));
void UsageFault_Handler(void) __attribute__((weak, alias("Default_Handler")));
void SVC_Handler(void) __attribute__((weak, alias("Default_Handler")));
void DebugMon_Handler(void) __attribute__((weak, alias("Default_Handler")));
void PendSV_Handler(void) __attribute__((weak, alias("Default_Handler")));
void SysTick_Handler(void) __attribute__((weak, alias("Default_Handler")));

__attribute__((section(".isr_vector"), used))
void (* const g_vector_table[TOTAL_VECTOR_COUNT])(void) =
{
    (void (*)(void))(&_estack), /* Vector 0: Initial stack pointer */
    Reset_Handler, /* Vector 1: Reset */
    NMI_Handler, /* Vector 2: NMI */
    HardFault_Handler, /* Vector 3: HardFault */
    MemManage_Handler, /* Vector 4: MemManage */
    BusFault_Handler, /* Vector 5: BusFault */
    UsageFault_Handler, /* Vector 6: UsageFault */
    Default_Handler, /* Vector 7: Reserved */
    Default_Handler, /* Vector 8: Reserved */
    Default_Handler, /* Vector 9: Reserved */
    Default_Handler, /* Vector 10: Reserved */
    SVC_Handler, /* Vector 11: SVCall */
    DebugMon_Handler, /* Vector 12: Debug monitor */
    Default_Handler, /* Vector 13: Reserved */
    PendSV_Handler, /* Vector 14: PendSV */
    SysTick_Handler, /* Vector 15: SysTick */
    Default_Handler, /* IRQ 0: default */
    Default_Handler, /* IRQ 1: default */
    Default_Handler, /* IRQ 2: default */
    Default_Handler, /* IRQ 3: default */
    Default_Handler, /* IRQ 4: default */
    Default_Handler, /* IRQ 5: default */
    Default_Handler, /* IRQ 6: default */
    Default_Handler, /* IRQ 7: default */
    Default_Handler, /* IRQ 8: default */
    Default_Handler, /* IRQ 9: default */
    Default_Handler, /* IRQ 10: default */
    Default_Handler, /* IRQ 11: default */
    Default_Handler, /* IRQ 12: default */
    Default_Handler, /* IRQ 13: default */
    Default_Handler, /* IRQ 14: default */
    Default_Handler, /* IRQ 15: default */
    Default_Handler, /* IRQ 16: default */
    Default_Handler, /* IRQ 17: default */
    Default_Handler, /* IRQ 18: default */
    Default_Handler, /* IRQ 19: default */
    Default_Handler, /* IRQ 20: default */
    Default_Handler, /* IRQ 21: default */
    Default_Handler, /* IRQ 22: default */
    Default_Handler, /* IRQ 23: default */
    Default_Handler, /* IRQ 24: default */
    Default_Handler, /* IRQ 25: default */
    Default_Handler, /* IRQ 26: default */
    Default_Handler, /* IRQ 27: default */
    Default_Handler, /* IRQ 28: default */
    Default_Handler, /* IRQ 29: default */
    Default_Handler, /* IRQ 30: default */
    Default_Handler, /* IRQ 31: default */
    Default_Handler, /* IRQ 32: default */
    Default_Handler, /* IRQ 33: default */
    Default_Handler, /* IRQ 34: default */
    Default_Handler, /* IRQ 35: default */
    Default_Handler, /* IRQ 36: default */
    Default_Handler, /* IRQ 37: default */
    USART2_IRQHandler, /* IRQ 38: USART2 */
    Default_Handler, /* IRQ 39: default */
    Default_Handler, /* IRQ 40: default */
    Default_Handler, /* IRQ 41: default */
    Default_Handler, /* IRQ 42: default */
    Default_Handler, /* IRQ 43: default */
    Default_Handler, /* IRQ 44: default */
    Default_Handler, /* IRQ 45: default */
    Default_Handler, /* IRQ 46: default */
    Default_Handler, /* IRQ 47: default */
    Default_Handler, /* IRQ 48: default */
    Default_Handler, /* IRQ 49: default */
    Default_Handler, /* IRQ 50: default */
    Default_Handler, /* IRQ 51: default */
    Default_Handler, /* IRQ 52: default */
    Default_Handler, /* IRQ 53: default */
    TIM6_DAC_IRQHandler, /* IRQ 54: TIM6_DAC */
    Default_Handler, /* IRQ 55: default */
    Default_Handler, /* IRQ 56: default */
    Default_Handler, /* IRQ 57: default */
    Default_Handler, /* IRQ 58: default */
    Default_Handler, /* IRQ 59: default */
    Default_Handler, /* IRQ 60: default */
    Default_Handler, /* IRQ 61: default */
    Default_Handler, /* IRQ 62: default */
    Default_Handler, /* IRQ 63: default */
    Default_Handler, /* IRQ 64: default */
    Default_Handler, /* IRQ 65: default */
    Default_Handler, /* IRQ 66: default */
    Default_Handler, /* IRQ 67: default */
    Default_Handler, /* IRQ 68: default */
    Default_Handler, /* IRQ 69: default */
    Default_Handler, /* IRQ 70: default */
    Default_Handler, /* IRQ 71: default */
    Default_Handler, /* IRQ 72: default */
    Default_Handler, /* IRQ 73: default */
    Default_Handler, /* IRQ 74: default */
    Default_Handler, /* IRQ 75: default */
    Default_Handler, /* IRQ 76: default */
    Default_Handler, /* IRQ 77: default */
    Default_Handler, /* IRQ 78: default */
    Default_Handler, /* IRQ 79: default */
    Default_Handler, /* IRQ 80: default */
    Default_Handler, /* IRQ 81: default */
    Default_Handler, /* IRQ 82: default */
    Default_Handler, /* IRQ 83: default */
    Default_Handler, /* IRQ 84: default */
    Default_Handler, /* IRQ 85: default */
    Default_Handler, /* IRQ 86: default */
    Default_Handler, /* IRQ 87: default */
    Default_Handler, /* IRQ 88: default */
    Default_Handler, /* IRQ 89: default */
    Default_Handler, /* IRQ 90: default */
    Default_Handler, /* IRQ 91: default */
    Default_Handler, /* IRQ 92: default */
    Default_Handler, /* IRQ 93: default */
    Default_Handler, /* IRQ 94: default */
    Default_Handler, /* IRQ 95: default */
    Default_Handler /* IRQ 96: default */
};

_Static_assert((sizeof(g_vector_table) / sizeof(g_vector_table[0])) == TOTAL_VECTOR_COUNT,
               "vector table size mismatch");

/**
 * @brief Initialize vector relocation, data and BSS sections, then call main.
 */
void Reset_Handler(void)
{
    uint32_t *source;
    uint32_t *destination;

    SCB_VTOR = (uint32_t)(uintptr_t)g_vector_table;
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

    (void)main();

    for (;;)
    {
        __asm volatile ("nop");
    }
}
