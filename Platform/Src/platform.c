#include "platform.h"

#include "app_event.h"
#include "stm32f446_minimal.h"

#define RCC_CR_HSION                     (1u << 0u)
#define RCC_CR_HSIRDY                    (1u << 1u)
#define RCC_CFGR_SW_MASK                 (3u << 0u)
#define RCC_CFGR_SWS_MASK                (3u << 2u)
#define RCC_CFGR_HPRE_MASK               (15u << 4u)
#define RCC_CFGR_PPRE1_MASK              (7u << 10u)
#define RCC_CFGR_PPRE2_MASK              (7u << 13u)
#define RCC_AHB1ENR_GPIOAEN              (1u << 0u)
#define RCC_APB1ENR_TIM6EN               (1u << 4u)
#define RCC_APB1RSTR_TIM6RST             (1u << 4u)

#define GPIO_MODE_OUTPUT                 (1u)
#define GPIO_MODE_ALTERNATE              (2u)
#define GPIO_ALTERNATE_FUNCTION_7        (7u)
#define GPIO_SPEED_HIGH                  (2u)
#define GPIO_PULL_UP                     (1u)

#define LED_PIN_NUMBER                   (5u)
#define USART_TX_PIN_NUMBER              (2u)
#define USART_RX_PIN_NUMBER              (3u)

#define TIM6_IRQ_NUMBER                  (54u)
#define TIM6_IRQ_PRIORITY                (6u)
#define TIM_DIER_UIE                     (1u << 0u)
#define TIM_SR_UIF                       (1u << 0u)
#define TIM_EGR_UG                       (1u << 0u)
#define TIM_CR1_CEN                      (1u << 0u)
#define TIM6_PRESCALER                   (PLATFORM_CORE_CLOCK_HZ / 1000u)
#define TIM6_PERIOD_COUNTS               (PLATFORM_TICK_PERIOD_MS)

/**
 * @brief Configure PA5 as output and PA2/PA3 as USART2 alternate-function pins.
 */
static void Platform_ConfigureGpio(void)
{
    uint32_t register_value;

    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
    (void)RCC->AHB1ENR;

    register_value = GPIOA->MODER;
    register_value &= ~((3u << (LED_PIN_NUMBER * 2u)) |
                        (3u << (USART_TX_PIN_NUMBER * 2u)) |
                        (3u << (USART_RX_PIN_NUMBER * 2u)));
    register_value |= ((GPIO_MODE_OUTPUT << (LED_PIN_NUMBER * 2u)) |
                       (GPIO_MODE_ALTERNATE << (USART_TX_PIN_NUMBER * 2u)) |
                       (GPIO_MODE_ALTERNATE << (USART_RX_PIN_NUMBER * 2u)));
    GPIOA->MODER = register_value;

    register_value = GPIOA->OSPEEDR;
    register_value &= ~((3u << (USART_TX_PIN_NUMBER * 2u)) |
                        (3u << (USART_RX_PIN_NUMBER * 2u)));
    register_value |= ((GPIO_SPEED_HIGH << (USART_TX_PIN_NUMBER * 2u)) |
                       (GPIO_SPEED_HIGH << (USART_RX_PIN_NUMBER * 2u)));
    GPIOA->OSPEEDR = register_value;

    register_value = GPIOA->PUPDR;
    register_value &= ~((3u << (USART_TX_PIN_NUMBER * 2u)) |
                        (3u << (USART_RX_PIN_NUMBER * 2u)));
    register_value |= (GPIO_PULL_UP << (USART_RX_PIN_NUMBER * 2u));
    GPIOA->PUPDR = register_value;

    register_value = GPIOA->AFR[0];
    register_value &= ~((15u << (USART_TX_PIN_NUMBER * 4u)) |
                        (15u << (USART_RX_PIN_NUMBER * 4u)));
    register_value |= ((GPIO_ALTERNATE_FUNCTION_7 << (USART_TX_PIN_NUMBER * 4u)) |
                       (GPIO_ALTERNATE_FUNCTION_7 << (USART_RX_PIN_NUMBER * 4u)));
    GPIOA->AFR[0] = register_value;

    Platform_LedSet(false);
}

/**
 * @brief Force the system clock to the reset-safe 16 MHz HSI source.
 */
static void Platform_ConfigureHsiClock(void)
{
    RCC->CR |= RCC_CR_HSION;
    while ((RCC->CR & RCC_CR_HSIRDY) == 0u)
    {
        __asm volatile ("nop");
    }

    RCC->CFGR &= ~RCC_CFGR_SW_MASK;

    while ((RCC->CFGR & RCC_CFGR_SWS_MASK) != 0u)
    {
        __asm volatile ("nop");
    }

    RCC->CFGR &= ~(RCC_CFGR_HPRE_MASK |
                   RCC_CFGR_PPRE1_MASK |
                   RCC_CFGR_PPRE2_MASK);
}

/**
 * @brief Initialize the HSI clock, GPIO, and board LED.
 */
void Platform_Init(void)
{
    Platform_ConfigureHsiClock();
    Platform_ConfigureGpio();
}

/**
 * @brief Configure and start TIM6 as a 5 ms application event source.
 */
void Platform_StartFiveMillisecondTimer(void)
{
    RCC->APB1ENR |= RCC_APB1ENR_TIM6EN;
    (void)RCC->APB1ENR;

    RCC->APB1RSTR |= RCC_APB1RSTR_TIM6RST;
    RCC->APB1RSTR &= ~RCC_APB1RSTR_TIM6RST;

    TIM6->CR1 = 0u;
    TIM6->PSC = TIM6_PRESCALER - 1u;
    TIM6->ARR = TIM6_PERIOD_COUNTS - 1u;
    TIM6->EGR = TIM_EGR_UG;
    TIM6->SR = 0u;
    TIM6->DIER = TIM_DIER_UIE;

    Stm32Nvic_SetPriority(TIM6_IRQ_NUMBER, TIM6_IRQ_PRIORITY);
    Stm32Nvic_EnableIrq(TIM6_IRQ_NUMBER);

    TIM6->CR1 = TIM_CR1_CEN;
}

/**
 * @brief Set the NUCLEO LD2 LED state.
 * @param is_on True turns the LED on; false turns it off.
 */
void Platform_LedSet(bool is_on)
{
    if (is_on == true)
    {
        GPIOA->BSRR = (1u << LED_PIN_NUMBER);
    }
    else
    {
        GPIOA->BSRR = (1u << (LED_PIN_NUMBER + 16u));
    }
}

/**
 * @brief Wait for an interrupt while preserving the caller's interrupt-mask state.
 * @note Call with interrupts masked only after atomically checking event state.
 */
void Platform_WaitForInterrupt(void)
{
    __asm volatile ("dsb" ::: "memory");
    __asm volatile ("wfi");
    __asm volatile ("isb" ::: "memory");
}

/**
 * @brief Disable interrupts and return the previous PRIMASK value.
 * @return Previous PRIMASK value.
 */
uint32_t Platform_IrqSave(void)
{
    uint32_t primask;
    __asm volatile ("mrs %0, primask" : "=r" (primask));
    __asm volatile ("cpsid i" ::: "memory");
    return primask;
}

/**
 * @brief Restore a previously saved PRIMASK value.
 * @param primask Value returned by Platform_IrqSave.
 */
void Platform_IrqRestore(uint32_t primask)
{
    __asm volatile ("msr primask, %0" :: "r" (primask) : "memory");
}

/**
 * @brief Handle the TIM6 update interrupt and post one 5 ms event.
 */
void TIM6_DAC_IRQHandler(void)
{
    if ((TIM6->SR & TIM_SR_UIF) != 0u)
    {
        TIM6->SR &= ~TIM_SR_UIF;
        AppEvent_PostTickFromIsr();
    }
}
