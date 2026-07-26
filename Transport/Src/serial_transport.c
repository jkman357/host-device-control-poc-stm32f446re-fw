// Copyright (c) 2026 Ray Yang. All rights reserved.
//
// File:
//     serial_transport.c
//
// Purpose:
//     Implements the bounded interrupt-driven USART2 transport adapter.
//
// Responsibilities:
//     - Configures USART2 for the selected compile-time Baud Rate profile.
//     - Maintains a fixed-capacity non-blocking transmit ring buffer.
//     - Posts received bytes and UART errors from interrupt context.
//     - Counts overflow and hardware errors without wrapping.
//
// Notes:
//     USART2 shares the TIM6 preemption priority to preserve event production order.

#include "serial_transport.h"

#include <limits.h>
#include <stddef.h>

#include "app_event.h"
#include "stm32f446_minimal.h"

#define RCC_APB1ENR_USART2EN (1u << 17u)
#define RCC_APB1RSTR_USART2RST (1u << 17u)

#define USART_SR_PE (1u << 0u)
#define USART_SR_FE (1u << 1u)
#define USART_SR_NE (1u << 2u)
#define USART_SR_ORE (1u << 3u)
#define USART_SR_RXNE (1u << 5u)
#define USART_SR_TXE (1u << 7u)
#define USART_SR_ERROR_MASK \
    (USART_SR_PE | USART_SR_FE | USART_SR_NE | USART_SR_ORE)

#define USART_CR1_RE (1u << 2u)
#define USART_CR1_TE (1u << 3u)
#define USART_CR1_RXNEIE (1u << 5u)
#define USART_CR1_TXEIE (1u << 7u)
#define USART_CR1_UE (1u << 13u)
#define USART_CR3_EIE (1u << 0u)

#define USART2_IRQ_NUMBER (38u)
#define USART2_IRQ_PRIORITY (5u)
#define USART2_BRR_VALUE \
    SERIAL_BAUD_BRR(SERIAL_BAUD_PERIPHERAL_CLOCK_HZ, \
                    SERIAL_TRANSPORT_BAUD_RATE)
#define USART2_BAUD_ERROR_PPM \
    SERIAL_BAUD_ERROR_PPM(SERIAL_BAUD_PERIPHERAL_CLOCK_HZ, \
                          SERIAL_TRANSPORT_BAUD_RATE)
#define USART_BRR_MAX_VALUE (0xFFFFu)
#define TX_RING_RESERVED_SLOT_COUNT (1u)

_Static_assert(SERIAL_BAUD_IS_SUPPORTED(SERIAL_TRANSPORT_BAUD_RATE) != 0u,
               "Unsupported USART2 baud rate.");
_Static_assert((USART2_BRR_VALUE > 0u) && (USART2_BRR_VALUE <= USART_BRR_MAX_VALUE),
               "USART2 BRR is outside the 16-bit register range.");
_Static_assert(USART2_BAUD_ERROR_PPM <= SERIAL_BAUD_MAX_ERROR_PPM,
               "USART2 baud error exceeds the allowed limit.");

#define TX_RING_MASK (SERIAL_TRANSPORT_TX_RING_CAPACITY - 1u)

#if ((SERIAL_TRANSPORT_TX_RING_CAPACITY & TX_RING_MASK) != 0u)
#error SERIAL_TRANSPORT_TX_RING_CAPACITY must be a power of two.
#endif

static volatile uint16_t s_tx_head;
static volatile uint16_t s_tx_tail;
static uint8_t s_tx_ring[SERIAL_TRANSPORT_TX_RING_CAPACITY];
static volatile uint32_t s_tx_overflow_count;
static volatile uint32_t s_uart_error_count;

/*
 * Function:
 *     serial_transport_enter_critical
 *
 * Purpose:
 *     Disables interrupts and captures the prior interrupt-mask state.
 *
 * Input Parameters:
 *     None.
 *
 * Output Parameters:
 *     None.
 *
 * Return Value:
 *     The prior PRIMASK value required by serial_transport_leave_critical.
 */
static uint32_t serial_transport_enter_critical(void)
{
    uint32_t primask;

    __asm volatile(
        "MRS %0, primask\n"
        "CPSID i"
        : "=r"(primask)
        :
        : "memory");

    return primask;
}

/*
 * Function:
 *     serial_transport_leave_critical
 *
 * Purpose:
 *     Restores the interrupt-mask state captured before a bounded critical section.
 *
 * Input Parameters:
 *     primask:
 *         PRIMASK value returned by serial_transport_enter_critical.
 *
 * Output Parameters:
 *     None.
 *
 * Return Value:
 *     None.
 */
static void serial_transport_leave_critical(uint32_t primask)
{
    __asm volatile("MSR primask, %0" : : "r"(primask) : "memory");
}

/*
 * Function:
 *     serial_transport_increment_saturated
 *
 * Purpose:
 *     Increments an interrupt-shared diagnostic counter without permitting wraparound.
 *
 * Input Parameters:
 *     value:
 *         Pointer to the volatile counter to inspect and update.
 *
 * Output Parameters:
 *     value:
 *         Receives the incremented value unless already saturated.
 *
 * Return Value:
 *     None.
 *
 * Notes:
 *     Called from interrupt or main context only under the documented ownership rules.
 */
static void serial_transport_increment_saturated(volatile uint32_t *value)
{
    if (*value < UINT32_MAX)
    {
        *value += 1u;
    }
}

/*
 * Function:
 *     serial_transport_init
 *
 * Purpose:
 *     Initializes the transmit queue, counters, and USART2 hardware profile.
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
void serial_transport_init(void)
{
    volatile uint32_t discard;

    s_tx_head = 0u;
    s_tx_tail = 0u;
    s_tx_overflow_count = 0u;
    s_uart_error_count = 0u;

    RCC->apb1enr |= RCC_APB1ENR_USART2EN;
    (void)RCC->apb1enr;

    RCC->apb1rstr |= RCC_APB1RSTR_USART2RST;
    RCC->apb1rstr &= ~RCC_APB1RSTR_USART2RST;

    USART2->cr1 = 0u;
    USART2->cr2 = 0u;
    USART2->cr3 = 0u;
    USART2->brr = USART2_BRR_VALUE;

    discard = USART2->sr;
    discard = USART2->dr;
    (void)discard;

    // Match TIM6 priority to serialize all ordered event-queue producers.
    stm32_nvic_set_priority(USART2_IRQ_NUMBER, USART2_IRQ_PRIORITY);
    stm32_nvic_enable_irq(USART2_IRQ_NUMBER);

    USART2->cr3 = USART_CR3_EIE;
    USART2->cr1 =
        USART_CR1_RE | USART_CR1_TE | USART_CR1_RXNEIE | USART_CR1_UE;
}

/*
 * Function:
 *     serial_transport_write
 *
 * Purpose:
 *     Queues a complete byte sequence for non-blocking USART2 transmission.
 *
 * Input Parameters:
 *     data:
 *         Pointer to bytes to queue.
 *     length:
 *         Number of bytes to queue.
 *
 * Output Parameters:
 *     None.
 *
 * Return Value:
 *     SERIAL_TRANSPORT_RESULT_OK:
 *         All bytes were queued.
 *     SERIAL_TRANSPORT_RESULT_INVALID_ARGUMENT:
 *         The data pointer was NULL for a nonzero length.
 *     SERIAL_TRANSPORT_RESULT_NO_CAPACITY:
 *         Available queue capacity was insufficient.
 *
 * Notes:
 *     Runs in main context and uses a bounded critical section.
 */
serial_transport_result_t serial_transport_write(
    const uint8_t *data,
    size_t length)
{
    uint16_t free_capacity;
    uint32_t primask;
    size_t index;

    if ((data == NULL) || (length == 0u) || (length > UINT16_MAX))
    {
        return SERIAL_TRANSPORT_RESULT_INVALID_ARGUMENT;
    }

    primask = serial_transport_enter_critical();
    free_capacity =
        (uint16_t)((s_tx_tail - s_tx_head - TX_RING_RESERVED_SLOT_COUNT) & TX_RING_MASK);

    if (length > free_capacity)
    {
        serial_transport_leave_critical(primask);
        serial_transport_increment_saturated(&s_tx_overflow_count);
        return SERIAL_TRANSPORT_RESULT_NO_CAPACITY;
    }

    for (index = 0u; index < length; index += 1u)
    {
        s_tx_ring[s_tx_head] = data[index];
        s_tx_head = (uint16_t)((s_tx_head + 1u) & TX_RING_MASK);
    }

    USART2->cr1 |= USART_CR1_TXEIE;
    serial_transport_leave_critical(primask);

    return SERIAL_TRANSPORT_RESULT_OK;
}

/*
 * Function:
 *     serial_transport_get_rx_overflow_count
 *
 * Purpose:
 *     Returns the saturated count of received bytes rejected by the event queue.
 *
 * Input Parameters:
 *     None.
 *
 * Output Parameters:
 *     None.
 *
 * Return Value:
 *     Current saturated receive-overflow count.
 */
uint32_t serial_transport_get_rx_overflow_count(void)
{
    return app_event_get_overflow_count();
}

/*
 * Function:
 *     serial_transport_get_tx_overflow_count
 *
 * Purpose:
 *     Returns the saturated count of transmit writes rejected for insufficient capacity.
 *
 * Input Parameters:
 *     None.
 *
 * Output Parameters:
 *     None.
 *
 * Return Value:
 *     Current saturated transmit-overflow count.
 */
uint32_t serial_transport_get_tx_overflow_count(void)
{
    return s_tx_overflow_count;
}

/*
 * Function:
 *     serial_transport_get_uart_error_count
 *
 * Purpose:
 *     Returns the saturated count of observed USART2 hardware errors.
 *
 * Input Parameters:
 *     None.
 *
 * Output Parameters:
 *     None.
 *
 * Return Value:
 *     Current saturated UART-error count.
 */
uint32_t serial_transport_get_uart_error_count(void)
{
    return s_uart_error_count;
}

/*
 * Function:
 *     USART2_IRQHandler
 *
 * Purpose:
 *     Services bounded USART2 receive, error, and transmit-ready conditions.
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
 *     Executes in interrupt context and does not block or parse protocol frames.
 */
void USART2_IRQHandler(void)
{
    uint32_t status;

    status = USART2->sr;

    if ((status & USART_SR_ERROR_MASK) != 0u)
    {
        volatile uint32_t discard;

        // Reading DR after SR clears PE, FE, NE, and ORE on the STM32F4 USART.
        discard = USART2->dr;
        (void)discard;

        serial_transport_increment_saturated(&s_uart_error_count);
        (void)app_event_post_uart_error_from_isr();
    }
    else if ((status & USART_SR_RXNE) != 0u)
    {
        uint8_t data_byte;

        data_byte = (uint8_t)USART2->dr;
        (void)app_event_post_rx_byte_from_isr(data_byte);
    }

    if (((status & USART_SR_TXE) != 0u) &&
        ((USART2->cr1 & USART_CR1_TXEIE) != 0u))
    {
        if (s_tx_tail != s_tx_head)
        {
            USART2->dr = s_tx_ring[s_tx_tail];
            s_tx_tail = (uint16_t)((s_tx_tail + 1u) & TX_RING_MASK);
        }
        else
        {
            USART2->cr1 &= ~USART_CR1_TXEIE;
        }
    }
}
