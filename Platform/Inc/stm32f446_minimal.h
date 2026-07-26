// Copyright (c) 2026 Ray Yang. All rights reserved.
//
// File:
//     stm32f446_minimal.h
//
// Purpose:
//     Defines the minimal STM32F446 register map used by the PoC.
//
// Public Contract:
//     - Defines only the register blocks and NVIC operations required by this firmware.
//     - Provides bounded inline helpers for interrupt enable and priority configuration.
//
// Notes:
//     This Product-owned minimal map is not a replacement for the complete vendor device header.

#ifndef STM32F446_MINIMAL_H
#define STM32F446_MINIMAL_H

#include <stdint.h>

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
} rcc_t;

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
} gpio_t;

typedef struct
{
    volatile uint32_t sr;
    volatile uint32_t dr;
    volatile uint32_t brr;
    volatile uint32_t cr1;
    volatile uint32_t cr2;
    volatile uint32_t cr3;
    volatile uint32_t gtpr;
} usart_t;

typedef struct
{
    volatile uint32_t cr1;
    volatile uint32_t cr2;
    volatile uint32_t smcr;
    volatile uint32_t dier;
    volatile uint32_t sr;
    volatile uint32_t egr;
    uint32_t reserved_0[3];
    volatile uint32_t cnt;
    volatile uint32_t psc;
    volatile uint32_t arr;
} tim_basic_t;

#define RCC ((rcc_t *)0x40023800u)
#define GPIOA ((gpio_t *)0x40020000u)
#define USART2 ((usart_t *)0x40004400u)
#define TIM6 ((tim_basic_t *)0x40001000u)
#define NVIC_ISER ((volatile uint32_t *)0xE000E100u)
#define NVIC_IPR ((volatile uint8_t *)0xE000E400u)

#define NVIC_INTERRUPTS_PER_SET_REGISTER (32u)
#define NVIC_PRIORITY_FIELD_SHIFT (4u)

/*
 * Function:
 *     stm32_nvic_enable_irq
 *
 * Purpose:
 *     Enables one NVIC interrupt line.
 *
 * Input Parameters:
 *     irq_number:
 *         Zero-based external interrupt number.
 *
 * Output Parameters:
 *     None.
 *
 * Return Value:
 *     None.
 */
static inline void stm32_nvic_enable_irq(uint32_t irq_number)
{
    NVIC_ISER[irq_number / NVIC_INTERRUPTS_PER_SET_REGISTER] =
        1u << (irq_number % NVIC_INTERRUPTS_PER_SET_REGISTER);
}

/*
 * Function:
 *     stm32_nvic_set_priority
 *
 * Purpose:
 *     Programs the implemented high-order priority bits for one NVIC interrupt.
 *
 * Input Parameters:
 *     irq_number:
 *         Zero-based external interrupt number.
 *     priority:
 *         Logical preemption-priority value supported by the target.
 *
 * Output Parameters:
 *     None.
 *
 * Return Value:
 *     None.
 */
static inline void stm32_nvic_set_priority(
    uint32_t irq_number,
    uint8_t priority)
{
    NVIC_IPR[irq_number] = (uint8_t)(priority << NVIC_PRIORITY_FIELD_SHIFT);
}

#endif
