// Copyright (c) 2026 Ray Yang. All rights reserved.
//
// File:
//     platform_stm32f446re.c
//
// Purpose:
//     Implements the STM32F446RE platform adapter.
//
// Responsibilities:
//     - Configures GPIOA, TIM6, and target interrupt routing.
//     - Controls the NUCLEO user LED.
//     - Posts bounded timer events from TIM6 interrupt context.
//
// Notes:
//     TIM6 and USART2 use equal preemption priority to preserve event order.

#include "platform.h"

#include "app_event.h"
#include "stm32f446_minimal.h"

#define RCC_AHB1ENR_GPIOAEN (1u << 0u)
#define RCC_APB1ENR_TIM6EN (1u << 4u)

#define GPIO_MODE_OUTPUT (1u)
#define GPIO_MODE_AF (2u)
#define GPIO_MODE_FIELD_MASK (3u)
#define GPIO_PIN2_SHIFT (4u)
#define GPIO_PIN3_SHIFT (6u)
#define GPIO_PIN5_SHIFT (10u)
#define GPIO_AF7 (7u)
#define GPIO_AF_FIELD_MASK (15u)
#define GPIO_AFRL_PIN2_SHIFT (8u)
#define GPIO_AFRL_PIN3_SHIFT (12u)
#define GPIO_USER_LED_PIN_NUMBER (5u)
#define GPIO_BSRR_RESET_SHIFT (16u)

#define TIM_CR1_CEN (1u << 0u)
#define TIM_CR1_ARPE (1u << 7u)
#define TIM_DIER_UIE (1u << 0u)
#define TIM_SR_UIF (1u << 0u)
#define TIM_EGR_UG (1u << 0u)
#define TIM6_DAC_IRQ_NUMBER (54u)
#define TIM6_DAC_IRQ_PRIORITY (5u)
#define TIM6_INPUT_CLOCK_HZ (16000000u)
#define TIM6_COUNTER_CLOCK_HZ (1000000u)
#define TIM6_PRESCALER_VALUE \
    ((TIM6_INPUT_CLOCK_HZ / TIM6_COUNTER_CLOCK_HZ) - 1u)
#define PLATFORM_INITIAL_SAMPLE_INTERVAL_US (5000u)
#define PLATFORM_MINIMUM_SAMPLE_INTERVAL_US (1u)

/*
 * Function:
 *     platform_init
 *
 * Purpose:
 *     Configures target GPIO, sample timer, and interrupt routing required by the PoC.
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
 *     Call once before enabling application processing.
 */
void platform_init(void)
{
    RCC->ahb1enr |= RCC_AHB1ENR_GPIOAEN;
    RCC->apb1enr |= RCC_APB1ENR_TIM6EN;
    (void)RCC->apb1enr;

    GPIOA->moder =
        (GPIOA->moder &
         ~((GPIO_MODE_FIELD_MASK << GPIO_PIN2_SHIFT) |
           (GPIO_MODE_FIELD_MASK << GPIO_PIN3_SHIFT) |
           (GPIO_MODE_FIELD_MASK << GPIO_PIN5_SHIFT))) |
        (GPIO_MODE_AF << GPIO_PIN2_SHIFT) |
        (GPIO_MODE_AF << GPIO_PIN3_SHIFT) |
        (GPIO_MODE_OUTPUT << GPIO_PIN5_SHIFT);

    GPIOA->afr[0] =
        (GPIOA->afr[0] &
         ~((GPIO_AF_FIELD_MASK << GPIO_AFRL_PIN2_SHIFT) |
           (GPIO_AF_FIELD_MASK << GPIO_AFRL_PIN3_SHIFT))) |
        (GPIO_AF7 << GPIO_AFRL_PIN2_SHIFT) |
        (GPIO_AF7 << GPIO_AFRL_PIN3_SHIFT);

    platform_led_set(false);

    TIM6->cr1 = 0u;
    // Divide the 16 MHz peripheral clock to a 1 MHz timer counter.
    TIM6->psc = TIM6_PRESCALER_VALUE;
    platform_sample_timer_set_interval_us(PLATFORM_INITIAL_SAMPLE_INTERVAL_US);
    TIM6->dier = TIM_DIER_UIE;

    // TIM6 and USART2 use the same preemption priority so their queue writes
    // cannot preempt one another and reverse the observed event order.
    stm32_nvic_set_priority(TIM6_DAC_IRQ_NUMBER, TIM6_DAC_IRQ_PRIORITY);
    stm32_nvic_enable_irq(TIM6_DAC_IRQ_NUMBER);

    TIM6->cr1 = TIM_CR1_ARPE | TIM_CR1_CEN;
}

/*
 * Function:
 *     platform_led_set
 *
 * Purpose:
 *     Sets the NUCLEO user LED to the requested logical state.
 *
 * Input Parameters:
 *     is_on:
 *         true turns the LED on; false turns it off.
 *
 * Output Parameters:
 *     None.
 *
 * Return Value:
 *     None.
 */
void platform_led_set(bool is_on)
{
    if (is_on == true)
    {
        GPIOA->bsrr = 1u << GPIO_USER_LED_PIN_NUMBER;
    }
    else
    {
        GPIOA->bsrr = 1u << (GPIO_USER_LED_PIN_NUMBER + GPIO_BSRR_RESET_SHIFT);
    }
}

/*
 * Function:
 *     platform_sample_timer_set_interval_us
 *
 * Purpose:
 *     Programs the TIM6 update interval in microseconds and restarts the timer period.
 *
 * Input Parameters:
 *     interval_us:
 *         Nonzero timer interval in microseconds.
 *
 * Output Parameters:
 *     None.
 *
 * Return Value:
 *     None.
 */
void platform_sample_timer_set_interval_us(uint16_t interval_us)
{
    if (interval_us < PLATFORM_MINIMUM_SAMPLE_INTERVAL_US)
    {
        interval_us = PLATFORM_MINIMUM_SAMPLE_INTERVAL_US;
    }

    TIM6->arr = (uint32_t)interval_us - 1u;
    TIM6->egr = TIM_EGR_UG;
    TIM6->sr = 0u;
}

/*
 * Function:
 *     TIM6_DAC_IRQHandler
 *
 * Purpose:
 *     Acknowledges a TIM6 update and posts one ordered sample-tick event.
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
 *     Executes in interrupt context and performs bounded non-blocking work.
 */
void TIM6_DAC_IRQHandler(void)
{
    if ((TIM6->sr & TIM_SR_UIF) != 0u)
    {
        TIM6->sr = 0u;
        (void)app_event_post_tick_from_isr();
    }
}
