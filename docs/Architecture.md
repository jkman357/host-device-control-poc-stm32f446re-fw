# Architecture

`Core` owns reset, main, and the explicit unsupported-newlib-syscall boundary. `Platform` owns GPIO/TIM6. `Transport` owns USART2, the compile-time baud profile and the TX ring. `App` owns the ordered ISR-to-main event queue, state machine, baud-dependent streaming policy and telemetry generation. `Protocol` owns framing and serialization.

The ordered event queue is single-producer by interrupt serialization and single-consumer in main. It is fixed at 256 entries; overflow is saturating and reflected through the RX-overflow telemetry status bit.

USART2 and TIM6 use the same NVIC preemption priority. This prevents one queue-producing ISR from preempting the other; their queue writes are therefore serialized.

## Baud profile boundary

`Transport/Inc/serial_baud.h` owns the supported 11-value baud list, default selection, BRR calculation, actual-rate calculation, error limit, command-only threshold and bandwidth formula. `Transport/Src/serial_transport.c` applies those values to USART2 and contains compile-time assertions for supported selection, BRR range and maximum baud error.

`App/Src/app.c` applies the transport profile to protocol behavior. It reports the profile-specific maximum stream rate, rejects intervals below the calculated minimum and disables streaming entirely at 1,200 through 9,600 baud. No protocol command dynamically changes USART2; the flashed firmware and PC application must be configured to the same baud before connection.

`Core/Src/syscalls.c` does not implement hosted I/O. It supplies strong failing definitions for the four syscall symbols referenced by newlib-nano so the linker does not select warning-bearing `libnosys` objects.

## Synthetic waveform generation

`App/Src/waveform_generator.c` is a stateless signal generator. It provides one-second sine, square and triangle periods and an integer-rounded 857,143 us ECG period corresponding to approximately 70 bpm. The sine path uses range reduction plus a seventh-order polynomial and does not require libm. The ECG path sums bounded smooth polynomial pulses for P, Q, R, S and T components.

`App/Src/app.c` owns the active waveform, waveform phase and 10-second segment elapsed time. START_STREAM resets all three to sine and zero. Each sample tick advances time, performs any segment transition, wraps the phase to the selected waveform period and serializes one float sample.
