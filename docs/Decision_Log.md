# Decision Log

## DEC-001 — Self-contained register-level PoC baseline

- **Date:** 2026-07-25
- **Status:** Proposed for human review
- **Decision:** Use a small project-owned STM32F446RE register layer instead of HAL/CubeMX-generated source for this first PoC package.
- **Rationale:** Keep the event, protocol, and transport boundaries visible and make the package buildable without importing generated middleware.
- **Consequence:** This repository has no `.ioc` file and is not CubeMX-regenerable. A later product-oriented baseline may replace `Platform` and `Transport` internals with HAL/LL while preserving their public interfaces.

## DEC-002 — Bare-metal event-driven execution

- **Date:** 2026-07-25
- **Status:** Proposed for human review
- **Decision:** Use interrupt publication plus a main-context event dispatcher; do not use an RTOS.
- **Rationale:** The PoC has one command channel, one 5 ms stream, and bounded responsibilities that do not require task scheduling.
- **Consequence:** All work must remain bounded; ISR/main synchronization and event-loss diagnostics are explicit.

## DEC-003 — 16-bit domain-based message identifiers

- **Date:** 2026-07-25
- **Status:** Proposed for human review
- **Decision:** Allocate health/system messages in `0x0000–0x00FF`, application control in `0x0100–0x0FFF`, and telemetry in `0x2000–0x2FFF`.
- **Rationale:** Preserve room for growth and align the PoC with the framework's message-domain guidance.
- **Consequence:** The PC application shall treat Message ID as a little-endian `uint16`.

## DEC-004 — Security excluded from the laboratory transport profile

- **Date:** 2026-07-25
- **Status:** Proposed for human review
- **Decision:** Do not add authentication, encryption, or a secure session to the direct-cable PoC profile.
- **Rationale:** This baseline validates architecture and data flow only.
- **Consequence:** The protocol must not be reused unchanged on an untrusted or network-reachable transport.
