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

#define RX_RING_CAPACITY                (256u)
#define TX_RING_CAPACITY                (512u)
#define RX_RING_MASK                    (RX_RING_CAPACITY - 1u)
#define TX_RING_MASK                    (TX_RING_CAPACITY - 1u)

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

/**
 * @brief Saturating increment for a transport diagnostic counter.
 * @param counter Counter to increment.
 */
static void SerialTransport_IncrementSaturatingU32(volatile uint32_t *counter)
{
    if (*counter < UINT32_MAX)
    {
        *counter += 1u;
    }
}

/**
 * @brief Calculate TX ring free capacity with one slot reserved.
 * @return Free byte capacity.
 */
static uint16_t SerialTransport_GetTxFreeCapacity(void)
{
    return (uint16_t)((s_tx_tail - s_tx_head - 1u) & TX_RING_MASK);
}

/**
 * @brief Push one received byte from interrupt context.
 * @param data_byte Received byte.
 */
static void SerialTransport_PushRxFromIsr(uint8_t data_byte)
{
    const uint16_t next_head = (uint16_t)((s_rx_head + 1u) & RX_RING_MASK);

    if (next_head == s_rx_tail)
    {
        SerialTransport_IncrementSaturatingU32(&s_rx_overflow_count);
    }
    else
    {
        s_rx_ring[s_rx_head] = data_byte;
        s_rx_head = next_head;
        AppEvent_PostFlagsFromIsr(APP_EVENT_FLAG_UART_RX_AVAILABLE);
    }
}

/**
 * @brief Initialize USART2 and static RX/TX ring buffers.
 */
void SerialTransport_Init(void)
{
    volatile uint32_t discarded_data;

    s_rx_head = 0u;
    s_rx_tail = 0u;
    s_tx_head = 0u;
    s_tx_tail = 0u;
    s_rx_overflow_count = 0u;
    s_tx_overflow_count = 0u;
    s_uart_error_count = 0u;

    RCC->APB1ENR |= RCC_APB1ENR_USART2EN;
    (void)RCC->APB1ENR;

    RCC->APB1RSTR |= RCC_APB1RSTR_USART2RST;
    RCC->APB1RSTR &= ~RCC_APB1RSTR_USART2RST;

    USART2->CR1 = 0u;
    USART2->CR2 = 0u;
    USART2->CR3 = 0u;
    USART2->BRR = USART2_BRR_16MHZ_115200;

    discarded_data = USART2->SR;
    discarded_data = USART2->DR;
    (void)discarded_data;

    Stm32Nvic_SetPriority(USART2_IRQ_NUMBER, USART2_IRQ_PRIORITY);
    Stm32Nvic_EnableIrq(USART2_IRQ_NUMBER);

    USART2->CR3 = USART_CR3_EIE;
    USART2->CR1 = USART_CR1_RE | USART_CR1_TE | USART_CR1_RXNEIE | USART_CR1_UE;
}

/**
 * @brief Read one byte from the RX ring buffer.
 * @param[out] data_byte Destination byte.
 * @return True when one byte was returned.
 */
bool SerialTransport_ReadByte(uint8_t *data_byte)
{
    if ((data_byte == NULL) || (s_rx_tail == s_rx_head))
    {
        return false;
    }

    *data_byte = s_rx_ring[s_rx_tail];
    s_rx_tail = (uint16_t)((s_rx_tail + 1u) & RX_RING_MASK);
    return true;
}

/**
 * @brief Queue a contiguous byte sequence for interrupt-driven transmission.
 * @param data Source bytes.
 * @param length Number of bytes.
 * @return Transport result.
 */
serial_transport_result_t SerialTransport_Write(const uint8_t *data, size_t length)
{
    uint32_t primask;
    size_t data_index;
    uint16_t free_capacity;

    if ((data == NULL) || (length == 0u) || (length >= TX_RING_CAPACITY))
    {
        return SERIAL_TRANSPORT_RESULT_INVALID_ARGUMENT;
    }

    primask = Platform_IrqSave();
    free_capacity = SerialTransport_GetTxFreeCapacity();

    if (length > (size_t)free_capacity)
    {
        SerialTransport_IncrementSaturatingU32(&s_tx_overflow_count);
        Platform_IrqRestore(primask);
        return SERIAL_TRANSPORT_RESULT_NO_CAPACITY;
    }

    data_index = 0u;
    while (data_index < length)
    {
        s_tx_ring[s_tx_head] = data[data_index];
        s_tx_head = (uint16_t)((s_tx_head + 1u) & TX_RING_MASK);
        data_index += 1u;
    }

    USART2->CR1 |= USART_CR1_TXEIE;
    Platform_IrqRestore(primask);

    return SERIAL_TRANSPORT_RESULT_OK;
}

/**
 * @brief Return a snapshot of transport diagnostic counters.
 * @param[out] statistics Destination statistics.
 */
void SerialTransport_GetStatistics(serial_transport_statistics_t *statistics)
{
    uint32_t primask;

    if (statistics != NULL)
    {
        primask = Platform_IrqSave();
        statistics->rx_overflow_count = s_rx_overflow_count;
        statistics->tx_overflow_count = s_tx_overflow_count;
        statistics->uart_error_count = s_uart_error_count;
        Platform_IrqRestore(primask);
    }
}

/**
 * @brief Handle USART2 RX, TX, and error interrupts.
 */
void USART2_IRQHandler(void)
{
    uint32_t status;
    uint8_t data_byte;

    status = USART2->SR;

    if ((status & USART_SR_ERROR_MASK) != 0u)
    {
        data_byte = (uint8_t)USART2->DR;
        (void)data_byte;
        SerialTransport_IncrementSaturatingU32(&s_uart_error_count);
        AppEvent_PostFlagsFromIsr(APP_EVENT_FLAG_UART_ERROR);
    }
    else if ((status & USART_SR_RXNE) != 0u)
    {
        data_byte = (uint8_t)USART2->DR;
        SerialTransport_PushRxFromIsr(data_byte);
    }
    else
    {
        /* No receive work. */
    }

    if (((status & USART_SR_TXE) != 0u) && ((USART2->CR1 & USART_CR1_TXEIE) != 0u))
    {
        if (s_tx_tail != s_tx_head)
        {
            USART2->DR = s_tx_ring[s_tx_tail];
            s_tx_tail = (uint16_t)((s_tx_tail + 1u) & TX_RING_MASK);
        }
        else
        {
            USART2->CR1 &= ~USART_CR1_TXEIE;
        }
    }
}
