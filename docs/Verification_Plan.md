# Verification Plan

1. Run validator and host tests.
2. Run independent Cortex-M4F clang/lld build.
3. Clean-build in STM32CubeIDE and confirm there are no `_close`, `_lseek`, `_read`, or `_write` linker diagnostics.
4. Flash the NUCLEO-F446RE.
5. Run strict Python command sweep at 1000, 5000 and 60000 us.
6. Confirm first sample is one, no gaps/status/parser errors, and STOP returns IDLE ACK.
7. Record firmware commit, PC tool commit, board identity and operator approval.
