# Firmware Architecture — USART2 Loopback Diagnostic

## Runtime model

```text
USART2 RX/error ISR
        |
        v
static RX ring + UART event flag
        |
        v
main superloop -> app_process_events()
        |
        v
read one byte -> queue the same byte in static TX ring
        |
        v
USART2 TXE ISR -> PA2 -> ST-LINK VCP -> Tera Term
```

The interrupt service routine does not echo directly. It moves one received byte into the bounded RX ring and
posts an event. Main context drains the RX ring and queues each unchanged byte into the TX ring. The TXE interrupt
then transmits queued bytes.

## Isolation boundary

The following retained modules are not active in this diagnostic runtime:

- Shared Protocol parser and command dispatcher
- ACK/NACK and Device Info generation
- TIM6 sample timer
- Sine generator
- Telemetry generation

They remain in the repository so the diagnostic can be based on the exact `dcffbd2` project structure without
mixing serial-path troubleshooting with a destructive Protocol rewrite.

## Concurrency boundaries

- USART2 ISR is the sole RX-ring producer.
- Main context is the sole RX-ring consumer.
- Main context is the sole TX-ring producer.
- USART2 ISR is the sole TX-ring consumer.
- Event flags are transferred through the existing bounded PRIMASK-protected snapshot.
- The application never blocks and does not busy-wait for TX completion.

## Diagnostic indication

LD2 toggles after `serial_transport_write()` accepts an echo byte. Therefore:

- LD2 toggle proves that the byte reached main-context loopback processing.
- Tera Term display proves that the return TX path also completed.

## Limitations

This diagnostic proves byte transport, not framing, CRC, command semantics, timing, or telemetry compatibility.
It is intended for individual keystrokes and short terminal input, not sustained throughput qualification.
