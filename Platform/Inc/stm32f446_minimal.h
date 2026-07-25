#ifndef STM32F446_MINIMAL_H
#define STM32F446_MINIMAL_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    volatile uint32_t MODER;
    volatile uint32_t OTYPER;
    volatile uint32_t OSPEEDR;
    volatile uint32_t PUPDR;
    volatile uint32_t IDR;
    volatile uint32_t ODR;
    volatile uint32_t BSRR;
    volatile uint32_t LCKR;
    volatile uint32_t AFR[2];
} gpio_registers_t;

typedef struct
{
    volatile uint32_t CR;
    volatile uint32_t PLLCFGR;
    volatile uint32_t CFGR;
    volatile uint32_t CIR;
    volatile uint32_t AHB1RSTR;
    volatile uint32_t AHB2RSTR;
    volatile uint32_t AHB3RSTR;
    uint32_t RESERVED0;
    volatile uint32_t APB1RSTR;
    volatile uint32_t APB2RSTR;
    uint32_t RESERVED1[2];
    volatile uint32_t AHB1ENR;
    volatile uint32_t AHB2ENR;
    volatile uint32_t AHB3ENR;
    uint32_t RESERVED2;
    volatile uint32_t APB1ENR;
    volatile uint32_t APB2ENR;
} rcc_registers_t;

typedef struct
{
    volatile uint32_t SR;
    volatile uint32_t DR;
    volatile uint32_t BRR;
    volatile uint32_t CR1;
    volatile uint32_t CR2;
    volatile uint32_t CR3;
    volatile uint32_t GTPR;
} usart_registers_t;

typedef struct
{
    volatile uint32_t CR1;
    volatile uint32_t CR2;
    uint32_t RESERVED0;
    volatile uint32_t DIER;
    volatile uint32_t SR;
    volatile uint32_t EGR;
    uint32_t RESERVED1[3];
    volatile uint32_t CNT;
    volatile uint32_t PSC;
    volatile uint32_t ARR;
} basic_timer_registers_t;

#define GPIOA_BASE_ADDRESS       (0x40020000u)
#define RCC_BASE_ADDRESS         (0x40023800u)
#define USART2_BASE_ADDRESS      (0x40004400u)
#define TIM6_BASE_ADDRESS        (0x40001000u)

#define GPIOA                    ((gpio_registers_t *)GPIOA_BASE_ADDRESS)
#define RCC                      ((rcc_registers_t *)RCC_BASE_ADDRESS)
#define USART2                   ((usart_registers_t *)USART2_BASE_ADDRESS)
#define TIM6                     ((basic_timer_registers_t *)TIM6_BASE_ADDRESS)

#define NVIC_ISER_BASE_ADDRESS   (0xE000E100u)
#define NVIC_ICER_BASE_ADDRESS   (0xE000E180u)
#define NVIC_IPR_BASE_ADDRESS    (0xE000E400u)

#define NVIC_ISER                ((volatile uint32_t *)NVIC_ISER_BASE_ADDRESS)
#define NVIC_ICER                ((volatile uint32_t *)NVIC_ICER_BASE_ADDRESS)
#define NVIC_IPR                 ((volatile uint8_t *)NVIC_IPR_BASE_ADDRESS)

/**
 * @brief Enable one external interrupt in the NVIC.
 * @param irq_number External interrupt number.
 */
static inline void Stm32Nvic_EnableIrq(uint32_t irq_number)
{
    const uint32_t register_index = irq_number >> 5u;
    const uint32_t bit_index = irq_number & 0x1Fu;
    NVIC_ISER[register_index] = (1u << bit_index);
}

/**
 * @brief Disable one external interrupt in the NVIC.
 * @param irq_number External interrupt number.
 */
static inline void Stm32Nvic_DisableIrq(uint32_t irq_number)
{
    const uint32_t register_index = irq_number >> 5u;
    const uint32_t bit_index = irq_number & 0x1Fu;
    NVIC_ICER[register_index] = (1u << bit_index);
}

/**
 * @brief Set one external interrupt priority.
 * @param irq_number External interrupt number.
 * @param priority Priority value in the implemented upper four bits.
 */
static inline void Stm32Nvic_SetPriority(uint32_t irq_number, uint8_t priority)
{
    NVIC_IPR[irq_number] = (uint8_t)(priority << 4u);
}

#ifdef __cplusplus
}
#endif

#endif /* STM32F446_MINIMAL_H */
