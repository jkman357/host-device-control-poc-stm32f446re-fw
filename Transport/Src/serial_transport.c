// Copyright (c) 2026 Ray Yang. All rights reserved.
//
// File:
//     serial_transport.c
//
// Purpose:
//     Implements interrupt-driven USART2 transport for ST-LINK VCP.
//
// Responsibilities:
//     - Owns fixed-capacity RX and TX ring buffers.
//     - Moves bytes in bounded interrupt handlers.
//     - Reports overflow and UART error counters.
//     - Separates transport behavior from application and protocol logic.

#include "serial_transport.h"

#include <limits.h>
#include <stddef.h>

#include "app_event.h"
#include "platform.h"
#include "stm32f446_minimal.h"

#define RCC_APB1ENR_USART2EN            (1u << 17u)
#define RCC_APB1RSTR_USART2RST          (1u << 17u)

#define USART_SR_PE                     (1u << 0u)
#define USART_SR_FE                     (1u << 1u)
#define USART_SR_NE                     (1u << 2u)
#define USART_SR_ORE                    (1u << 3u)
#define USART_SR_RXNE                   (1u << 5u)
#define USART_SR_TXE                    (1u << 7u)
#define USART_SR_ERROR_MASK             (USART_SR_PE | USART_SR_FE | USART_SR_NE | USART_SR_ORE)

#define USART_CR1_RE                    (1u << 2u)
#define USART_CR1_TE                    (1u << 3u)
#define USART_CR1_RXNEIE                (1u << 5u)
#define USART_CR1_TXEIE                 (1u << 7u)
#define USART_CR1_UE                    (1u << 13u)
#define USART_CR3_EIE                   (1u << 0u)

#define USART2_IRQ_NUMBER               (38u)
#define USART2_IRQ_PRIORITY             (5u)
#define USART2_BRR_16MHZ_115200         (0x008Bu)

#define RX_RING_CAPACITY                (2048u)
#define TX_RING_CAPACITY                (2048u)
#define RX_RING_MASK                    (RX_RING_CAPACITY - 1u)
#define TX_RING_MASK                    (TX_RING_CAPACITY - 1u)
#define RING_RESERVED_SLOT_COUNT         (1u)

#if ((RX_RING_CAPACITY & RX_RING_MASK) != 0u)
#error RX_RING_CAPACITY must be a power of two.
#endif

#if ((TX_RING_CAPACITY & TX_RING_MASK) != 0u)
#error TX_RING_CAPACITY must be a power of two.
#endif

static volatile uint16_t s_rx_head;
static volatile uint16_t s_rx_tail;
static volatile uint16_t s_tx_head;
static volatile uint16_t s_tx_tail;
static uint8_t s_rx_ring[RX_RING_CAPACITY];
static uint8_t s_tx_ring[TX_RING_CAPACITY];
static volatile uint32_t s_rx_overflow_count;
static volatile uint32_t s_tx_overflow_count;
static volatile uint32_t s_uart_error_count;

/*
 * Function:
 *     serial_transport_increment_saturating_u32
 *
 * Purpose:
 *     Saturating increment for a transport diagnostic counter.
 *
 * Input Parameters:
 *     counter:
 *         Counter to increment.
 *
 * Output Parameters:
 *     None.
 *
 * Return Value:
 *     None.
 */
static void serial_transport_increment_saturating_u32(volatile uint32_t *counter)
{
    if (*counter < UINT32_MAX)
    {
        *counter += 1u;
    }
}

/*
 * Function:
 *     serial_transport_get_tx_free_capacity
 *
 * Purpose:
 *     Calculate TX ring free capacity with one slot reserved.
 *
 * Input Parameters:
 *     None.
 *
 * Output Parameters:
 *     None.
 *
 * Return Value:
 *     result:
 *         Free byte capacity.
 */
static uint16_t serial_transport_get_tx_free_capacity(void)
{
    return (uint16_t)((s_tx_tail - s_tx_head - RING_RESERVED_SLOT_COUNT) & TX_RING_MASK);
}

/*
 * Function:
 *     serial_transport_push_rx_from_isr
 *
 * Purpose:
 *     Stores one received byte in the bounded receive ring.
 *
 * Input Parameters:
 *     data_byte:
 *         Supplies the byte read from USART2.
 *
 * Output Parameters:
 *     None.
 *
 * Return Value:
 *     None.
 *
 * Notes:
 *     Execution Context: ISR. Blocking: prohibited. On overflow the byte
 *         is dropped and a saturating counter is incremented.
 */
static void serial_transport_push_rx_from_isr(uint8_t data_byte)
{
    const uint16_t next_head = (uint16_t)((s_rx_head + 1u) & RX_RING_MASK);

    if (next_head == s_rx_tail)
    {
        serial_transport_increment_saturating_u32(&s_rx_overflow_count);
    }
    else
    {
        s_rx_ring[s_rx_head] = data_byte;
        s_rx_head = next_head;
        app_event_post_flags_from_isr(APP_EVENT_FLAG_UART_RX_AVAILABLE);
    }
}

/*
 * Function:
 *     serial_transport_init
 *
 * Purpose:
 *     Initialize USART2 and static RX/TX ring buffers.
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
void serial_transport_init(void)
{
    volatile uint32_t discarded_data;

    s_rx_head = 0u;
    s_rx_tail = 0u;
    s_tx_head = 0u;
    s_tx_tail = 0u;
    s_rx_overflow_count = 0u;
    s_tx_overflow_count = 0u;
    s_uart_error_count = 0u;

    RCC->apb1enr |= RCC_APB1ENR_USART2EN;
    // Read back the enable register to complete the peripheral-clock write.
    (void)RCC->apb1enr;

    RCC->apb1rstr |= RCC_APB1RSTR_USART2RST;
    RCC->apb1rstr &= ~RCC_APB1RSTR_USART2RST;

    USART2->cr1 = 0u;
    USART2->cr2 = 0u;
    USART2->cr3 = 0u;
    USART2->brr = USART2_BRR_16MHZ_115200;

    discarded_data = USART2->sr;
    discarded_data = USART2->dr;
    // The read sequence clears any reset-time receive and error status.
    (void)discarded_data;

    stm32_nvic_set_priority(USART2_IRQ_NUMBER, USART2_IRQ_PRIORITY);
    stm32_nvic_enable_irq(USART2_IRQ_NUMBER);

    USART2->cr3 = USART_CR3_EIE;
    USART2->cr1 = USART_CR1_RE | USART_CR1_TE | USART_CR1_RXNEIE | USART_CR1_UE;
}

/*
 * Function:
 *     serial_transport_read_byte
 *
 * Purpose:
 *     Removes one byte from the receive ring buffer.
 *
 * Input Parameters:
 *     None.
 *
 * Output Parameters:
 *     data_byte:
 *         Receives one byte when true is returned. The object remains
 *         unchanged when false is returned.
 *
 * Return Value:
 *     true:
 *         One byte was returned.
 *     false:
 *         The pointer was NULL or the receive ring was empty.
 *
 * Notes:
 *     Runs in main context and does not block.
 */
bool serial_transport_read_byte(uint8_t *data_byte)
{
    if ((data_byte == NULL) || (s_rx_tail == s_rx_head))
    {
        return false;
    }

    *data_byte = s_rx_ring[s_rx_tail];
    s_rx_tail = (uint16_t)((s_rx_tail + 1u) & RX_RING_MASK);
    return true;
}

/*
 * Function:
 *     serial_transport_write
 *
 * Purpose:
 *     Queues a contiguous byte sequence for interrupt-driven
 *         transmission.
 *
 * Input Parameters:
 *     data:
 *         Points to the bytes to queue. The function copies all bytes
 *         before returning.
 *     length:
 *         Supplies the number of bytes to queue.
 *
 * Output Parameters:
 *     None.
 *
 * Return Value:
 *     SERIAL_TRANSPORT_RESULT_OK:
 *         All bytes were queued.
 *     SERIAL_TRANSPORT_RESULT_INVALID_ARGUMENT:
 *         The pointer or length was invalid.
 *     SERIAL_TRANSPORT_RESULT_NO_CAPACITY:
 *         The transmit ring did not have sufficient capacity.
 *
 * Notes:
 *     Runs in main context and uses a bounded PRIMASK critical section.
 */
serial_transport_result_t serial_transport_write(const uint8_t *data, size_t length)
{
    uint32_t primask;
    size_t data_index;
    uint16_t free_capacity;

    if ((data == NULL) || (length == 0u) || (length >= TX_RING_CAPACITY))
    {
        return SERIAL_TRANSPORT_RESULT_INVALID_ARGUMENT;
    }

    primask = platform_irq_save();
    free_capacity = serial_transport_get_tx_free_capacity();

    if (length > (size_t)free_capacity)
    {
        serial_transport_increment_saturating_u32(&s_tx_overflow_count);
        platform_irq_restore(primask);
        return SERIAL_TRANSPORT_RESULT_NO_CAPACITY;
    }

    data_index = 0u;
    while (data_index < length)
    {
        s_tx_ring[s_tx_head] = data[data_index];
        s_tx_head = (uint16_t)((s_tx_head + 1u) & TX_RING_MASK);
        data_index += 1u;
    }

    USART2->cr1 |= USART_CR1_TXEIE;
    platform_irq_restore(primask);

    return SERIAL_TRANSPORT_RESULT_OK;
}

/*
 * Function:
 *     serial_transport_get_statistics
 *
 * Purpose:
 *     Returns an atomic snapshot of transport diagnostic counters.
 *
 * Input Parameters:
 *     None.
 *
 * Output Parameters:
 *     statistics:
 *         Receives the counter snapshot when the pointer is valid. A NULL
 *         pointer is ignored.
 *
 * Return Value:
 *     None.
 *
 * Notes:
 *     Runs in main context and uses a bounded PRIMASK critical section.
 */
void serial_transport_get_statistics(serial_transport_statistics_t *statistics)
{
    uint32_t primask;

    if (statistics != NULL)
    {
        primask = platform_irq_save();
        statistics->rx_overflow_count = s_rx_overflow_count;
        statistics->tx_overflow_count = s_tx_overflow_count;
        statistics->uart_error_count = s_uart_error_count;
        platform_irq_restore(primask);
    }
}

/*
 * Function:
 *     serial_transport_usart2_irq_handler
 *
 * Purpose:
 *     Handles USART2 receive, transmit, and error interrupts.
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
 *     Execution Context: ISR. Blocking: prohibited. Reentrant: not
 *         required. Timing Budget: one received byte and one transmitted
 *         byte per invocation. No application or protocol processing
 *         occurs.
 */
void serial_transport_usart2_irq_handler(void)
{
    uint32_t status;
    uint8_t data_byte;

    status = USART2->sr;

    if ((status & USART_SR_ERROR_MASK) != 0u)
    {
        data_byte = (uint8_t)USART2->dr;
        // Reading the data register clears the detected receive error condition.
        (void)data_byte;
        serial_transport_increment_saturating_u32(&s_uart_error_count);
        app_event_post_flags_from_isr(APP_EVENT_FLAG_UART_ERROR);
    }
    else if ((status & USART_SR_RXNE) != 0u)
    {
        data_byte = (uint8_t)USART2->dr;
        serial_transport_push_rx_from_isr(data_byte);
    }
    else
    {
        // No receive work.
    }

    if (((status & USART_SR_TXE) != 0u) && ((USART2->cr1 & USART_CR1_TXEIE) != 0u))
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
