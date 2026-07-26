# Verification Plan

1. Run repository validator, host tests and validator mutation tests.
2. Build all 11 compile-time baud profiles with Cortex-M4F Clang/LLD.
3. Confirm the baud-profile host test verifies the shared list, BRR values, error ppm, command-only classification and minimum intervals.
4. Confirm application policy tests at 9,600, 19,200, 460,800 and 921,600 verify DEVICE_INFO, NACK behavior and minimum-interval enforcement.
5. Run waveform generator and 10-second application rotation host tests.
6. For each baud profile, perform an STM32CubeIDE clean build with the selected `SERIAL_TRANSPORT_BAUD_RATE` visible in the build log.
7. Flash the NUCLEO-F446RE and confirm PING plus GET_DEVICE_INFO at the matching PC baud.
8. At 1,200 through 9,600, confirm GET_DEVICE_INFO reports 0 Hz and SET_STREAM_CONFIG / START_STREAM return NACK with INVALID_STATE.
9. At each streaming profile, run the strict Python sweep at the profile minimum interval and at 60,000 us.
10. At 460,800 and 921,600, additionally run 1,000 us and 5,000 us cases.
11. Confirm first sample is one, no gaps/status/parser errors, and STOP returns IDLE ACK.
12. Observe at least one full 40-second waveform cycle at a supported streaming interval and confirm sine → square → triangle → ECG 70 bpm → sine.
13. Record firmware commit, selected compile-time baud, PC tool commit, board identity, ST-LINK version, operator, duration and evidence path in `docs/Hardware_Baud_Test_Record.md`.
