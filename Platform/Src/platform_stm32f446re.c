// Copyright (c) 2026 Ray Yang. All rights reserved.

#include "platform.h"

#include "app_event.h"
#include "stm32f446_minimal.h"

#define RCC_AHB1ENR_GPIOAEN (1u << 0u)
#define RCC_APB1ENR_TIM6EN (1u << 4u)

#define GPIO_MODE_OUTPUT (1u)
#define GPIO_MODE_AF (2u)
#define GPIO_PIN2_SHIFT (4u)
#define GPIO_PIN3_SHIFT (6u)
#define GPIO_PIN5_SHIFT (10u)
#define GPIO_AF7 (7u)
#define GPIO_AFRL_PIN2_SHIFT (8u)
#define GPIO_AFRL_PIN3_SHIFT (12u)

#define TIM_CR1_CEN (1u << 0u)
#define TIM_CR1_ARPE (1u << 7u)
#define TIM_DIER_UIE (1u << 0u)
#define TIM_SR_UIF (1u << 0u)
#define TIM_EGR_UG (1u << 0u)
#define TIM6_DAC_IRQ_NUMBER (54u)
#define TIM6_DAC_IRQ_PRIORITY (5u)

void platform_init(void)
{
    RCC->ahb1enr |= RCC_AHB1ENR_GPIOAEN;
    RCC->apb1enr |= RCC_APB1ENR_TIM6EN;
    (void)RCC->apb1enr;

    GPIOA->moder =
        (GPIOA->moder &
         ~((3u << GPIO_PIN2_SHIFT) |
           (3u << GPIO_PIN3_SHIFT) |
           (3u << GPIO_PIN5_SHIFT))) |
        (GPIO_MODE_AF << GPIO_PIN2_SHIFT) |
        (GPIO_MODE_AF << GPIO_PIN3_SHIFT) |
        (GPIO_MODE_OUTPUT << GPIO_PIN5_SHIFT);

    GPIOA->afr[0] =
        (GPIOA->afr[0] &
         ~((15u << GPIO_AFRL_PIN2_SHIFT) |
           (15u << GPIO_AFRL_PIN3_SHIFT))) |
        (GPIO_AF7 << GPIO_AFRL_PIN2_SHIFT) |
        (GPIO_AF7 << GPIO_AFRL_PIN3_SHIFT);

    platform_led_set(false);

    TIM6->cr1 = 0u;
    TIM6->psc = 15u; /* 16 MHz / 16 = 1 MHz timer counter. */
    platform_sample_timer_set_interval_us(5000u);
    TIM6->dier = TIM_DIER_UIE;

    /*
     * TIM6 and USART2 use the same preemption priority so their queue writes
     * cannot preempt one another and reverse the observed event order.
     */
    stm32_nvic_set_priority(TIM6_DAC_IRQ_NUMBER, TIM6_DAC_IRQ_PRIORITY);
    stm32_nvic_enable_irq(TIM6_DAC_IRQ_NUMBER);

    TIM6->cr1 = TIM_CR1_ARPE | TIM_CR1_CEN;
}

void platform_led_set(bool on)
{
    if (on)
    {
        GPIOA->bsrr = 1u << 5u;
    }
    else
    {
        GPIOA->bsrr = 1u << (5u + 16u);
    }
}

void platform_sample_timer_set_interval_us(uint16_t interval_us)
{
    TIM6->arr = (uint32_t)interval_us - 1u;
    TIM6->egr = TIM_EGR_UG;
    TIM6->sr = 0u;
}

void TIM6_DAC_IRQHandler(void)
{
    if ((TIM6->sr & TIM_SR_UIF) != 0u)
    {
        TIM6->sr = 0u;
        (void)app_event_post_tick_from_isr();
    }
}
