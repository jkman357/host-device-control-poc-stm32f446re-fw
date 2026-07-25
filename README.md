# STM32F446RE USART2 Character Loopback Test Firmware

Temporary diagnostic firmware derived from MCU commit `dcffbd2`. It isolates the physical and software serial
path before the C# application and Shared Protocol are used.

## Test objective

Each byte received through the NUCLEO-F446RE ST-LINK Virtual COM Port is returned unchanged:

```text
Tera Term -> ST-LINK VCP -> PA3 / USART2 RX
          -> RX ring -> main-context loopback
          -> TX ring -> PA2 / USART2 TX -> Tera Term
```

Expected behavior:

```text
Type a -> Tera Term displays a
Type b -> Tera Term displays b
```

LD2 toggles after every byte successfully queued for echo. This provides a second indication that the MCU
received and processed the character.

## Runtime scope

- NUCLEO-F446RE / STM32F446RET6
- Internal HSI at 16 MHz
- USART2 at 115200 bps, 8-N-1, no flow control
- PA2: USART2 TX to ST-LINK VCP
- PA3: USART2 RX from ST-LINK VCP
- PA5: LD2 diagnostic toggle
- Interrupt-driven RX and TX static ring buffers
- Event-driven foreground processing
- No RTOS, heap, blocking UART call, TIM6 sampling, Protocol parser, or telemetry

The Shared Protocol files and tests remain in the repository as the retained `dcffbd2` baseline, but this test
firmware intentionally bypasses them at runtime. It must not be tagged as a Protocol-compatible release.

## STM32CubeIDE build and flash

1. Preserve `.git/` when replacing an existing working tree.
2. Delete generated `Debug/` and `Release/` directories.
3. Replace the remaining files with this package.
4. Refresh or freshly import the project in STM32CubeIDE.
5. Select **Project -> Clean** and build **Debug**.
6. Flash with **Run -> Run As -> STM32 C/C++ Application**.
7. Confirm the download completes, then close any debugger session that still owns the target.

## Tera Term setup

1. Connect the NUCLEO board through the ST-LINK USB connector.
2. Open the ST-LINK Virtual COM Port in Tera Term.
3. Configure:

```text
Baud rate:    115200
Data bits:    8
Parity:       none
Stop bits:    1
Flow control: none
Local echo:   OFF
```

4. Press the `a` key once. One `a` shall appear.
5. Press the `b` key once. One `b` shall appear.
6. Continue with letters, numbers, and Enter to confirm byte-for-byte echo.

Local echo must be OFF. When local echo is ON, Tera Term displays the typed character immediately and then
displays the MCU-returned character, causing each character to appear twice.

## Result interpretation

| Observation | Interpretation |
|---|---|
| Character echoes and LD2 toggles | PC, COM port, ST-LINK VCP, PA2/PA3, USART2 IRQ, RX/TX rings, and main loop work |
| No character and no LD2 toggle | RX path, selected COM port, firmware download, or USART2 interrupt path is not working |
| LD2 toggles but no character returns | MCU RX/main path works; investigate USART2 TX, PA2, VCP return path, or terminal ownership |
| Character appears twice | Tera Term local echo is enabled |
| Some characters are missing during a large paste | Test exceeds the intended single-character diagnostic scope or TX capacity |

## Validation

```bash
python3 Tools/validate_project.py
make host-test
python3 Tools/serial_smoke_test.py --self-test
python3 Tools/test_linker_layout.py
bash Tools/build_with_clang.sh
```

The Protocol tests remain useful regression checks for the retained source baseline, but they do not exercise the
temporary loopback runtime path. Final evidence is the physical Tera Term echo test.

## Status

Temporary UART loopback diagnostic derived from commit `dcffbd2`. After the serial path is proven, restore the
normal Shared Protocol runtime before resuming PC/MCU interoperability work.
