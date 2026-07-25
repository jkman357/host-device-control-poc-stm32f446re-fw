// Copyright (c) 2026 Ray Yang. All rights reserved.
//
// File:
//     platform.c
//
// Purpose:
//     Implements STM32F446 board and processor services.
//
// Responsibilities:
//     - Configures the reset-safe HSI clock with bounded readiness checks.
//     - Configures NUCLEO-F446RE GPIO and microsecond-resolution TIM6.
//     - Provides LED, interrupt-mask, and wait primitives.
//     - Posts sample-timer events from interrupt context.

#include "platform.h"

#include "app_event.h"
#include "stm32f446_minimal.h"

#define RCC_CR_HSION                         (1u << 0u)
#define RCC_CR_HSIRDY                        (1u << 1u)
#define RCC_CFGR_SW_MASK                     (3u << 0u)
#define RCC_CFGR_SWS_MASK                    (3u << 2u)
#define RCC_CFGR_HPRE_MASK                   (15u << 4u)
#define RCC_CFGR_PPRE1_MASK                  (7u << 10u)
#define RCC_CFGR_PPRE2_MASK                  (7u << 13u)
#define RCC_AHB1ENR_GPIOAEN                  (1u << 0u)
#define RCC_APB1ENR_TIM6EN                   (1u << 4u)
#define RCC_APB1RSTR_TIM6RST                 (1u << 4u)

#define GPIO_MODE_OUTPUT                     (1u)
#define GPIO_MODE_ALTERNATE                  (2u)
#define GPIO_ALTERNATE_FUNCTION_7            (7u)
#define GPIO_SPEED_HIGH                      (2u)
#define GPIO_PULL_UP                         (1u)
#define GPIO_MODE_FIELD_MASK                 (3u)
#define GPIO_MODE_FIELD_WIDTH                (2u)
#define GPIO_ALTERNATE_FIELD_MASK            (15u)
#define GPIO_ALTERNATE_FIELD_WIDTH           (4u)
#define GPIO_ALTERNATE_LOW_REGISTER_INDEX    (0u)
#define GPIO_BSRR_RESET_SHIFT                (16u)

#define LED_PIN_NUMBER                       (5u)
#define USART_TX_PIN_NUMBER                  (2u)
#define USART_RX_PIN_NUMBER                  (3u)

#define TIM6_IRQ_NUMBER                      (54u)
#define TIM6_IRQ_PRIORITY                    (6u)
#define TIM_DIER_UIE                         (1u << 0u)
#define TIM_SR_UIF                           (1u << 0u)
#define TIM_EGR_UG                           (1u << 0u)
#define TIM_CR1_CEN                          (1u << 0u)
#define MICROSECONDS_PER_SECOND              (1000000u)
#define TIM6_COUNTER_CLOCK_HZ                (1000000u)
#define TIM6_PRESCALER                       (PLATFORM_CORE_CLOCK_HZ / TIM6_COUNTER_CLOCK_HZ)
#define TIM6_PERIOD_MIN_US                   (1u)
#define PLATFORM_CLOCK_READY_POLL_LIMIT      (100000u)

/*
 * Function:
 *     platform_configure_gpio
 *
 * Purpose:
 *     Configures PA5 as output and PA2/PA3 as USART2 alternate-function pins.
 *
 * Input Parameters:
 *     None.
 *
 * Output Parameters:
 *     None.
 *
 * Return Value:
 *     None.
 */
static void platform_configure_gpio(void)
{
    uint32_t register_value;

    RCC->ahb1enr |= RCC_AHB1ENR_GPIOAEN;
    // Read back the enable register to complete the peripheral-clock write.
    (void)RCC->ahb1enr;

    register_value = GPIOA->moder;
    register_value &= ~((GPIO_MODE_FIELD_MASK << (LED_PIN_NUMBER * GPIO_MODE_FIELD_WIDTH)) |
                        (GPIO_MODE_FIELD_MASK << (USART_TX_PIN_NUMBER * GPIO_MODE_FIELD_WIDTH)) |
                        (GPIO_MODE_FIELD_MASK << (USART_RX_PIN_NUMBER * GPIO_MODE_FIELD_WIDTH)));
    register_value |= ((GPIO_MODE_OUTPUT << (LED_PIN_NUMBER * GPIO_MODE_FIELD_WIDTH)) |
                       (GPIO_MODE_ALTERNATE << (USART_TX_PIN_NUMBER * GPIO_MODE_FIELD_WIDTH)) |
                       (GPIO_MODE_ALTERNATE << (USART_RX_PIN_NUMBER * GPIO_MODE_FIELD_WIDTH)));
    GPIOA->moder = register_value;

    register_value = GPIOA->ospeedr;
    register_value &= ~((GPIO_MODE_FIELD_MASK << (USART_TX_PIN_NUMBER * GPIO_MODE_FIELD_WIDTH)) |
                        (GPIO_MODE_FIELD_MASK << (USART_RX_PIN_NUMBER * GPIO_MODE_FIELD_WIDTH)));
    register_value |= ((GPIO_SPEED_HIGH << (USART_TX_PIN_NUMBER * GPIO_MODE_FIELD_WIDTH)) |
                       (GPIO_SPEED_HIGH << (USART_RX_PIN_NUMBER * GPIO_MODE_FIELD_WIDTH)));
    GPIOA->ospeedr = register_value;

    register_value = GPIOA->pupdr;
    register_value &= ~((GPIO_MODE_FIELD_MASK << (USART_TX_PIN_NUMBER * GPIO_MODE_FIELD_WIDTH)) |
                        (GPIO_MODE_FIELD_MASK << (USART_RX_PIN_NUMBER * GPIO_MODE_FIELD_WIDTH)));
    register_value |= (GPIO_PULL_UP << (USART_RX_PIN_NUMBER * GPIO_MODE_FIELD_WIDTH));
    GPIOA->pupdr = register_value;

    register_value = GPIOA->afr[GPIO_ALTERNATE_LOW_REGISTER_INDEX];
    register_value &=
        ~((GPIO_ALTERNATE_FIELD_MASK << (USART_TX_PIN_NUMBER * GPIO_ALTERNATE_FIELD_WIDTH)) |
          (GPIO_ALTERNATE_FIELD_MASK << (USART_RX_PIN_NUMBER * GPIO_ALTERNATE_FIELD_WIDTH)));
    register_value |=
        ((GPIO_ALTERNATE_FUNCTION_7 << (USART_TX_PIN_NUMBER * GPIO_ALTERNATE_FIELD_WIDTH)) |
         (GPIO_ALTERNATE_FUNCTION_7 << (USART_RX_PIN_NUMBER * GPIO_ALTERNATE_FIELD_WIDTH)));
    GPIOA->afr[GPIO_ALTERNATE_LOW_REGISTER_INDEX] = register_value;

    platform_led_set(false);
}

/*
 * Function:
 *     platform_configure_hsi_clock
 *
 * Purpose:
 *     Selects the reset-safe 16 MHz HSI source with bounded readiness polling.
 *
 * Input Parameters:
 *     None.
 *
 * Output Parameters:
 *     None.
 *
 * Return Value:
 *     true:
 *         HSI became ready and was selected.
 *     false:
 *         Readiness or source selection exceeded the poll limit.
 */
static bool platform_configure_hsi_clock(void)
{
    uint32_t poll_count;

    RCC->cr |= RCC_CR_HSION;
    poll_count = 0u;
    while (((RCC->cr & RCC_CR_HSIRDY) == 0u) &&
           (poll_count < PLATFORM_CLOCK_READY_POLL_LIMIT))
    {
        poll_count += 1u;
    }

    if ((RCC->cr & RCC_CR_HSIRDY) == 0u)
    {
        return false;
    }

    RCC->cfgr &= ~RCC_CFGR_SW_MASK;
    poll_count = 0u;
    while (((RCC->cfgr & RCC_CFGR_SWS_MASK) != 0u) &&
           (poll_count < PLATFORM_CLOCK_READY_POLL_LIMIT))
    {
        poll_count += 1u;
    }

    if ((RCC->cfgr & RCC_CFGR_SWS_MASK) != 0u)
    {
        return false;
    }

    RCC->cfgr &= ~(RCC_CFGR_HPRE_MASK |
                   RCC_CFGR_PPRE1_MASK |
                   RCC_CFGR_PPRE2_MASK);

    return true;
}

/*
 * Function:
 *     platform_apply_sample_period_us
 *
 * Purpose:
 *     Applies one valid TIM6 period with a bounded interrupt critical section.
 *
 * Input Parameters:
 *     period_us:
 *         Supplies a validated period from one through 65535 microseconds.
 *
 * Output Parameters:
 *     None.
 *
 * Return Value:
 *     None.
 */
static void platform_apply_sample_period_us(uint16_t period_us)
{
    uint32_t primask;

    primask = platform_irq_save();
    TIM6->cr1 = 0u;
    TIM6->psc = TIM6_PRESCALER - 1u;
    TIM6->arr = (uint32_t)period_us - 1u;
    TIM6->egr = TIM_EGR_UG;
    TIM6->sr = 0u;
    TIM6->dier = TIM_DIER_UIE;
    TIM6->cr1 = TIM_CR1_CEN;
    platform_irq_restore(primask);
}

/*
 * Function:
 *     platform_init
 *
 * Purpose:
 *     Initializes the reset-safe HSI clock, GPIO, and board LED.
 *
 * Input Parameters:
 *     None.
 *
 * Output Parameters:
 *     None.
 *
 * Return Value:
 *     true:
 *         Required initialization completed.
 *     false:
 *         HSI initialization exceeded the poll limit.
 */
bool platform_init(void)
{
    bool is_initialized;

    is_initialized = platform_configure_hsi_clock();
    if (is_initialized == true)
    {
        platform_configure_gpio();
    }

    return is_initialized;
}

/*
 * Function:
 *     platform_start_sample_timer
 *
 * Purpose:
 *     Configures and starts TIM6 for the requested sample period.
 *
 * Input Parameters:
 *     period_us:
 *         Supplies a timer period from one through 65535 microseconds.
 *
 * Output Parameters:
 *     None.
 *
 * Return Value:
 *     true:
 *         TIM6 was configured and started.
 *     false:
 *         The requested period was outside the hardware range.
 */
bool platform_start_sample_timer(uint16_t period_us)
{
    if (period_us < TIM6_PERIOD_MIN_US)
    {
        return false;
    }

    RCC->apb1enr |= RCC_APB1ENR_TIM6EN;
    // Read back the enable register to complete the peripheral-clock write.
    (void)RCC->apb1enr;

    RCC->apb1rstr |= RCC_APB1RSTR_TIM6RST;
    RCC->apb1rstr &= ~RCC_APB1RSTR_TIM6RST;

    stm32_nvic_set_priority(TIM6_IRQ_NUMBER, TIM6_IRQ_PRIORITY);
    stm32_nvic_enable_irq(TIM6_IRQ_NUMBER);
    platform_apply_sample_period_us(period_us);

    return true;
}

/*
 * Function:
 *     platform_set_sample_period_us
 *
 * Purpose:
 *     Atomically reconfigures a running TIM6 sample period.
 *
 * Input Parameters:
 *     period_us:
 *         Supplies a timer period from one through 65535 microseconds.
 *
 * Output Parameters:
 *     None.
 *
 * Return Value:
 *     true:
 *         The period was applied.
 *     false:
 *         The requested period was outside the hardware range.
 */
bool platform_set_sample_period_us(uint16_t period_us)
{
    if (period_us < TIM6_PERIOD_MIN_US)
    {
        return false;
    }

    platform_apply_sample_period_us(period_us);
    return true;
}

/*
 * Function:
 *     platform_led_set
 *
 * Purpose:
 *     Sets the NUCLEO LD2 LED state.
 *
 * Input Parameters:
 *     is_on:
 *         True turns the LED on; false turns it off.
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
        GPIOA->bsrr = (1u << LED_PIN_NUMBER);
    }
    else
    {
        GPIOA->bsrr = (1u << (LED_PIN_NUMBER + GPIO_BSRR_RESET_SHIFT));
    }
}

/*
 * Function:
 *     platform_wait_for_interrupt
 *
 * Purpose:
 *     Waits for an interrupt using the caller-controlled interrupt-mask protocol.
 *
 * Input Parameters:
 *     None.
 *
 * Output Parameters:
 *     None.
 *
 * Return Value:
 *     None.
 */
void platform_wait_for_interrupt(void)
{
    __asm volatile ("dsb" ::: "memory");
    __asm volatile ("wfi");
    __asm volatile ("isb" ::: "memory");
}

/*
 * Function:
 *     platform_irq_save
 *
 * Purpose:
 *     Disables maskable interrupts and returns the previous PRIMASK state.
 *
 * Input Parameters:
 *     None.
 *
 * Output Parameters:
 *     None.
 *
 * Return Value:
 *     primask:
 *         Previous PRIMASK value.
 */
uint32_t platform_irq_save(void)
{
    uint32_t primask;

    __asm volatile ("mrs %0, primask" : "=r" (primask));
    __asm volatile ("cpsid i" ::: "memory");

    return primask;
}

/*
 * Function:
 *     platform_irq_restore
 *
 * Purpose:
 *     Restores a PRIMASK state previously returned by platform_irq_save.
 *
 * Input Parameters:
 *     primask:
 *         Supplies the PRIMASK value to restore.
 *
 * Output Parameters:
 *     None.
 *
 * Return Value:
 *     None.
 */
void platform_irq_restore(uint32_t primask)
{
    __asm volatile ("msr primask, %0" :: "r" (primask) : "memory");
}

/*
 * Function:
 *     platform_tim6_dac_irq_handler
 *
 * Purpose:
 *     Handles a TIM6 update interrupt and posts one sample-timer event.
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
 *     Execution Context: ISR. Blocking: prohibited. Work is bounded.
 */
void platform_tim6_dac_irq_handler(void)
{
    if ((TIM6->sr & TIM_SR_UIF) != 0u)
    {
        TIM6->sr &= ~TIM_SR_UIF;
        app_event_post_tick_from_isr();
    }
}
