# Hardware Baud Rate Test Record

## Status meaning

- **Prior evidence**: evidence from an earlier firmware baseline; useful for continuity but not qualification of v0.2.7.
- **Not executed**: no target-board result is claimed.
- **Pass / Fail**: may only be entered after the stated firmware build, PC build, board identity and test evidence are recorded.

## Prior evidence received

| Date | Firmware | Baud | Stream | Evidence | Result | Qualification scope |
|---|---|---:|---:|---|---|---|
| 2026-07-26 | 0.2.5 | 460,800 | 5,000 us / 200 Hz | `docs/evidence/2026-07-26_460800_200hz_prior-baseline.png`: 1,090 samples, CRC/format/partial/unknown all zero, lost/UI drops zero | Prior evidence | Confirms earlier 460,800 interoperability only; not a v0.2.7 all-profile qualification |

## v0.2.7 profile execution record

| Baud | Expected mode | Required interval checks | STM32CubeIDE clean build | Flash / handshake | Strict sweep | Long run | Status |
|---:|---|---|---|---|---|---|---|
| 1,200 | Command-only | SET/START must NACK | Not executed | Not executed | Command-only sweep not executed | N/A | Not executed |
| 2,400 | Command-only | SET/START must NACK | Not executed | Not executed | Command-only sweep not executed | N/A | Not executed |
| 4,800 | Command-only | SET/START must NACK | Not executed | Not executed | Command-only sweep not executed | N/A | Not executed |
| 9,600 | Command-only | SET/START must NACK | Not executed | Not executed | Command-only sweep not executed | N/A | Not executed |
| 19,200 | Stream | 15,000 and 60,000 us | Not executed | Not executed | Not executed | Not executed | Not executed |
| 38,400 | Stream | 7,507 and 60,000 us | Not executed | Not executed | Not executed | Not executed | Not executed |
| 57,600 | Stream | 5,005 and 60,000 us | Not executed | Not executed | Not executed | Not executed | Not executed |
| 115,200 | Stream | 2,503 and 60,000 us | Not executed | Not executed | Not executed | Not executed | Not executed |
| 230,400 | Stream | 1,250 and 60,000 us | Not executed | Not executed | Not executed | Not executed | Not executed |
| 460,800 | Stream | 1,000, 5,000 and 60,000 us | Not executed | Not executed | Not executed | Not executed | Not executed |
| 921,600 | Stream | 1,000, 5,000 and 60,000 us | Not executed | Not executed | Not executed | Not executed | Not executed |

## Required evidence per tested profile

Record all of the following before changing a row to Pass:

1. Firmware commit and `SERIAL_TRANSPORT_BAUD_RATE` build value.
2. STM32CubeIDE version, GNU Arm toolchain version and clean-build log.
3. Board identity and ST-LINK firmware version.
4. PC application or Python tool commit and selected COM port/baud.
5. PING and GET_DEVICE_INFO result.
6. Expected command-only NACK behavior, or strict streaming sweep at the profile minimum interval.
7. CRC, format, status, sample-gap and UART overflow counts.
8. Test duration, operator, date and evidence path.
