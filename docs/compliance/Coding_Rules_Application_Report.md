# Coding Rules Application Report

## Baseline

- Coding standard: `Embedded_C_Coding_Rules.md` v1.0.17, Final Baseline
- Framework reference commit: `7a68980ef5faa2e897a3574af121683d65f74638`
- Coding-rules application baseline: MCU commit `4b1b701`
- Current firmware implementation baseline: v0.2.1

## Applied controls

The repository applies the following high-value mechanical and implementation controls:

- Standardized Product-owned `.c` and `.h` File Headers with the approved copyright holder.
- Mandatory Function Headers on Product-owned declarations and definitions.
- Lower snake case function names with module prefixes.
- `s_` prefixes for file-static objects and `g_` prefixes for global objects.
- A Global Object Register for the interrupt vector table.
- A Deviation Record for toolchain-mandated external symbols.
- Named constants for protocol fields, hardware registers, timing, capacities, and test expectations.
- Explicit return-value handling at module boundaries.
- Static memory only, bounded buffers, bounded ISR work, and no RTOS.
- Zero-error and zero-warning build expectations.
- Automated validation for headers, comments, names, prohibited APIs, Protocol authority, vectors, target metadata,
  and linker layout.

## Protocol-change review

The v0.2.0 wire-protocol replacement remains intact in v0.2.1. The Coding Rules controls continue to cover
framing, Message IDs, payloads, timer configuration, and telemetry representation. The mechanical validator still checks all 24
Product-owned C/header files and rejects reintroduction of legacy wire markers.

## Compliance statement

This baseline is designed to comply with the Project-specific Embedded C Coding Rules v1.0.17 for the implemented
scope. It does not by itself establish MISRA C:2023 compliance. Full MISRA claims require the additional evidence
identified by the coding standard, including static-analysis and guideline-compliance records.
