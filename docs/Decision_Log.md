# Decision Log

## DEC-001 — Self-contained register-level PoC baseline

- **Date:** 2026-07-25
- **Status:** Proposed for human review
- **Decision:** Use a small project-owned STM32F446RE register layer instead of HAL/CubeMX-generated source for this first PoC package.
- **Rationale:** Keep event, protocol, and transport boundaries visible and make the package buildable without importing generated middleware.
- **Consequence:** This repository has no `.ioc` file and is not CubeMX-regenerable. A later product-oriented baseline may replace `Platform` and `Transport` internals while preserving their public interfaces.

## DEC-002 — Bare-metal event-driven execution

- **Date:** 2026-07-25
- **Status:** Proposed for human review
- **Decision:** Use interrupt publication plus a main-context event dispatcher; do not use an RTOS.
- **Rationale:** The PoC has one command channel, one configurable stream, and bounded responsibilities that do not require task scheduling.
- **Consequence:** All work must remain bounded; ISR/main synchronization and event-loss diagnostics are explicit.

## DEC-003 — Superseded MCU-local Message ID allocation

- **Date:** 2026-07-25
- **Status:** Superseded by DEC-005
- **Former decision:** Use 16-bit, domain-based MCU-local Message IDs.
- **Reason superseded:** The system repository now provides the authoritative shared PC/MCU contract with one-byte Message IDs and fixed semantics. An implementation-local allocation cannot override it.
- **Consequence:** No legacy domain-based IDs or flags field remain in the MCU wire implementation.

## DEC-004 — Security excluded from the laboratory transport profile

- **Date:** 2026-07-25
- **Status:** Proposed for human review
- **Decision:** Do not add authentication, encryption, or a secure session to the direct-cable PoC profile.
- **Rationale:** This baseline validates architecture and data flow only.
- **Consequence:** The protocol must not be reused unchanged on an untrusted or network-reachable transport.

## DEC-005 — System repository owns the shared Protocol contract

- **Date:** 2026-07-25
- **Status:** Applied; lifecycle approval remains external
- **Decision:** Treat `host-device-control-poc-system/protocol/protocol.yaml` as the sole wire-level authority and implement its pinned Protocol 0.1.0 snapshot.
- **Rationale:** PC and MCU require one implementation-neutral contract. The authoritative rule is `specification_precedes_implementation`.
- **Consequence:** The MCU code consumes the contract; changes begin in the system repository and shared vectors, then propagate to both implementations.

## DEC-006 — Do not invent DEVICE_STATUS or ERROR_REPORT semantics

- **Date:** 2026-07-25
- **Status:** Applied
- **Decision:** Retain the declared Message IDs and payload definitions but do not emit these messages in v0.2.1.
- **Rationale:** Protocol 0.1.0 does not define a `DEVICE_STATUS` trigger or an `ERROR_REPORT` error-code allocation and emission policy.
- **Consequence:** Adding those behaviors requires a contract-first change rather than an MCU-only interpretation.

## DEC-007 — Configurable sample timer remains active in IDLE

- **Date:** 2026-07-25
- **Status:** Applied
- **Decision:** Run TIM6 continuously and permit `SET_STREAM_CONFIG` only in IDLE.
- **Rationale:** The same timer supports device time, parser timeout progress, heartbeat timing, and stream scheduling without adding a second time base.
- **Consequence:** `device_tick_us` represents modulo-`uint32` device time, not time since stream start. START_STREAM resets stream counters and phase but not device time.
