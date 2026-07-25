# Build and Flash

## STM32CubeIDE

The repository includes STM32CubeIDE project metadata and uses its managed GNU Arm toolchain.

1. Import the repository root as an existing project.
2. Select `Debug`.
3. Build.
4. Create a normal STM32 Cortex-M C/C++ debug configuration for NUCLEO-F446RE if the IDE does not automatically create one.
5. Program the generated ELF through the onboard ST-LINK.

## GNU Make

```bash
make clean
make
```

The default command expects:

```text
arm-none-eabi-gcc
arm-none-eabi-objcopy
arm-none-eabi-size
```

## Validation without the GNU Arm toolchain

The project includes two checks that can run independently:

```bash
python3 Tools/validate_project.py
make host-test
```

For environments with Clang and LLD:

```bash
Tools/build_with_clang.sh
```

That script performs a complete Cortex-M4 compile and link check without using STM32 HAL or CMSIS libraries.


## Serial smoke test

After flashing, install pyserial and run the included host-side smoke test:

```bash
python3 -m pip install pyserial
python3 Tools/serial_smoke_test.py COM5
```

On Linux, replace `COM5` with the VCP device such as `/dev/ttyACM0`. The script sends `PING`, `GET_DEVICE_INFO`, `START_STREAM`, collects telemetry, then sends `STOP_STREAM`.

The protocol-only path can be checked without hardware or pyserial:

```bash
python3 Tools/serial_smoke_test.py --self-test
```
