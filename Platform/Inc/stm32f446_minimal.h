// Copyright (c) 2026 Ray Yang. All rights reserved.
//
// File:
//     stm32f446_minimal.h
//
// Purpose:
//     Defines the minimal STM32F446 register model used by the PoC.
//
// Public Contract:
//     - Provides only the peripheral register layouts and addresses required by this firmware.
//     - Provides bounded Nested Vectored Interrupt Controller helper functions.
//     - Avoids a dependency on a vendor hardware-abstraction library.

#ifndef STM32F446_MINIMAL_H
#define STM32F446_MINIMAL_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GPIOA_BASE_ADDRESS             (0x40020000u)
#define RCC_BASE_ADDRESS               (0x40023800u)
#define USART2_BASE_ADDRESS            (0x40004400u)
#define TIM6_BASE_ADDRESS              (0x40001000u)
#define NVIC_ISER_BASE_ADDRESS         (0xE000E100u)
#define NVIC_ICER_BASE_ADDRESS         (0xE000E180u)
#define NVIC_IPR_BASE_ADDRESS          (0xE000E400u)
#define NVIC_REGISTER_INDEX_SHIFT      (5u)
#define NVIC_REGISTER_BIT_MASK         (0x1Fu)
#define NVIC_PRIORITY_FIELD_SHIFT      (4u)

typedef struct
{
    volatile uint32_t moder;
    volatile uint32_t otyper;
    volatile uint32_t ospeedr;
    volatile uint32_t pupdr;
    volatile uint32_t idr;
    volatile uint32_t odr;
    volatile uint32_t bsrr;
    volatile uint32_t lckr;
    volatile uint32_t afr[2];
} gpio_registers_t;

typedef struct
{
    volatile uint32_t cr;
    volatile uint32_t pllcfgr;
    volatile uint32_t cfgr;
    volatile uint32_t cir;
    volatile uint32_t ahb1rstr;
    volatile uint32_t ahb2rstr;
    volatile uint32_t ahb3rstr;
    uint32_t reserved_0;
    volatile uint32_t apb1rstr;
    volatile uint32_t apb2rstr;
    uint32_t reserved_1[2];
    volatile uint32_t ahb1enr;
    volatile uint32_t ahb2enr;
    volatile uint32_t ahb3enr;
    uint32_t reserved_2;
    volatile uint32_t apb1enr;
    volatile uint32_t apb2enr;
} rcc_registers_t;

typedef struct
{
    volatile uint32_t sr;
    volatile uint32_t dr;
    volatile uint32_t brr;
    volatile uint32_t cr1;
    volatile uint32_t cr2;
    volatile uint32_t cr3;
    volatile uint32_t gtpr;
} usart_registers_t;

typedef struct
{
    volatile uint32_t cr1;
    volatile uint32_t cr2;
    uint32_t reserved_0;
    volatile uint32_t dier;
    volatile uint32_t sr;
    volatile uint32_t egr;
    uint32_t reserved_1[3];
    volatile uint32_t cnt;
    volatile uint32_t psc;
    volatile uint32_t arr;
} basic_timer_registers_t;

#define GPIOA    ((gpio_registers_t *)GPIOA_BASE_ADDRESS)
#define RCC      ((rcc_registers_t *)RCC_BASE_ADDRESS)
#define USART2   ((usart_registers_t *)USART2_BASE_ADDRESS)
#define TIM6     ((basic_timer_registers_t *)TIM6_BASE_ADDRESS)

#define NVIC_ISER    ((volatile uint32_t *)NVIC_ISER_BASE_ADDRESS)
#define NVIC_ICER    ((volatile uint32_t *)NVIC_ICER_BASE_ADDRESS)
#define NVIC_IPR     ((volatile uint8_t *)NVIC_IPR_BASE_ADDRESS)

/*
 * Function:
 *     stm32_nvic_enable_irq
 *
 * Purpose:
 *     Enables one external interrupt in the Nested Vectored Interrupt Controller.
 *
 * Input Parameters:
 *     irq_number:
 *         Supplies the external interrupt number.
 *
 * Output Parameters:
 *     None.
 *
 * Return Value:
 *     None.
 */
static inline void stm32_nvic_enable_irq(uint32_t irq_number)
{
    uint32_t register_index;
    uint32_t bit_index;

    register_index = irq_number >> NVIC_REGISTER_INDEX_SHIFT;
    bit_index = irq_number & NVIC_REGISTER_BIT_MASK;
    NVIC_ISER[register_index] = (1u << bit_index);
}

/*
 * Function:
 *     stm32_nvic_disable_irq
 *
 * Purpose:
 *     Disables one external interrupt in the Nested Vectored Interrupt Controller.
 *
 * Input Parameters:
 *     irq_number:
 *         Supplies the external interrupt number.
 *
 * Output Parameters:
 *     None.
 *
 * Return Value:
 *     None.
 */
static inline void stm32_nvic_disable_irq(uint32_t irq_number)
{
    uint32_t register_index;
    uint32_t bit_index;

    register_index = irq_number >> NVIC_REGISTER_INDEX_SHIFT;
    bit_index = irq_number & NVIC_REGISTER_BIT_MASK;
    NVIC_ICER[register_index] = (1u << bit_index);
}

/*
 * Function:
 *     stm32_nvic_set_priority
 *
 * Purpose:
 *     Sets one external interrupt priority in the implemented upper priority bits.
 *
 * Input Parameters:
 *     irq_number:
 *         Supplies the external interrupt number.
 *     priority:
 *         Supplies the logical priority value.
 *
 * Output Parameters:
 *     None.
 *
 * Return Value:
 *     None.
 */
static inline void stm32_nvic_set_priority(uint32_t irq_number, uint8_t priority)
{
    NVIC_IPR[irq_number] = (uint8_t)(priority << NVIC_PRIORITY_FIELD_SHIFT);
}

#ifdef __cplusplus
}
#endif

#endif // STM32F446_MINIMAL_H
