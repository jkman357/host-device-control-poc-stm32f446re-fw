// Copyright (c) 2026 Ray Yang. All rights reserved.

#include <stdint.h>

typedef void (*isr_handler_t)(void);

extern int main(void);
extern void USART2_IRQHandler(void);
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

static void startup_default_handler(void)
{
    for (;;)
    {
    }
}

static void startup_enable_fpu(void)
{
    SCB_CPACR |=
        SCB_CPACR_CP10_FULL_ACCESS | SCB_CPACR_CP11_FULL_ACCESS;

    /* Complete CPACR write before any hard-float instruction can execute. */
    __asm volatile("DSB" : : : "memory");
    __asm volatile("ISB" : : : "memory");
}

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

__attribute__((section(".isr_vector"), used))
const isr_handler_t g_vector_table[16u + 82u] = {
    [0] = (isr_handler_t)&_estack,
    [1] = startup_reset_handler,
    [2 ... 15] = startup_default_handler,
    [16u + 38u] = USART2_IRQHandler,
    [16u + 54u] = TIM6_DAC_IRQHandler,
};
