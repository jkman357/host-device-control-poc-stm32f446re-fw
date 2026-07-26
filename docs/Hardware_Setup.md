# Hardware Setup

Use a NUCLEO-F446RE and the onboard ST-LINK VCP. USART2 is PA2 TX / PA3 RX, 8-N-1.

The baud is selected at firmware compile time through `SERIAL_TRANSPORT_BAUD_RATE`; default is 460,800. Set the PC application to the exact same value before connection. Changing only the PC selection does not change the MCU.

Supported values are 1,200, 2,400, 4,800, 9,600, 19,200, 38,400, 57,600, 115,200, 230,400, 460,800 and 921,600. Close other terminal programs before starting the Python sweep.

See `docs/Baud_Rate_Profiles.md` for stream limits and `docs/Hardware_Baud_Test_Record.md` for required evidence.
