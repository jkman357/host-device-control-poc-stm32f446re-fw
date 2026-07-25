# USART2 Character Loopback Verification Plan

## Build gate

1. Delete generated `Debug/` and `Release/` directories.
2. Clean-build Debug in STM32CubeIDE.
3. Require zero errors and zero warnings.
4. Confirm the ELF, MAP, and LIST files are generated.
5. Flash the generated image through ST-LINK.

## Terminal configuration

```text
Port:         STMicroelectronics STLink Virtual COM Port
Baud:         115200
Data bits:    8
Parity:       none
Stop bits:    1
Flow control: none
Local echo:   OFF
```

## Manual test cases

### LB-001 Single character `a`

1. Open Tera Term on the board VCP.
2. Press `a` once.
3. Verify exactly one `a` appears.
4. Verify LD2 changes state once.

### LB-002 Single character `b`

1. Press `b` once.
2. Verify exactly one `b` appears.
3. Verify LD2 changes state once.

### LB-003 Character sequence

1. Type `abc123` slowly.
2. Verify the terminal displays `abc123` in the same order.
3. Verify no extra characters are inserted.

### LB-004 Enter key

1. Press Enter.
2. Verify the configured terminal line-ending byte or bytes are echoed.
3. Accept visual cursor movement according to Tera Term receive-newline settings.

### LB-005 Reset recovery

1. Close Tera Term or release the COM port.
2. Reset or reflash the board.
3. Reopen the same VCP.
4. Repeat LB-001 and LB-002.

## Diagnostic checkpoints

When the test fails, set breakpoints in this order:

1. `serial_transport_usart2_irq_handler()`
2. `serial_transport_read_byte()`
3. `app_echo_received_bytes()`
4. `serial_transport_write()`

The first function not reached identifies the broken boundary.

## Exit criterion

The loopback diagnostic passes when individual characters are returned byte-for-byte, in order, with Tera Term
local echo disabled and with a matching LD2 toggle for each successfully queued byte.

A pass does not approve the Shared Protocol. It only permits restoration of the normal `dcffbd2` Protocol runtime
and continuation of framed PC/MCU interoperability testing.
