# Architecture

`Core` owns reset, main, and the explicit unsupported-newlib-syscall boundary. `Platform` owns GPIO/TIM6. `Transport` owns USART2 and the TX ring. `App` owns the ordered ISR-to-main event queue, state machine and telemetry generation. `Protocol` owns framing and serialization.

The ordered event queue is single-producer by interrupt serialization and single-consumer in main. It is fixed at 256 entries; overflow is saturating and reflected through the RX-overflow telemetry status bit.

USART2 and TIM6 use the same NVIC preemption priority. This prevents one queue-producing ISR from preempting the other; their queue writes are therefore serialized.

`Core/Src/syscalls.c` does not implement hosted I/O. It supplies strong failing definitions for the four syscall symbols referenced by newlib-nano so the linker does not select warning-bearing `libnosys` objects.
