# Protocol Authority Record

## Source identity

| Field | Value |
|---|---|
| System repository | `jkman357/host-device-control-poc-system` |
| Authoritative branch | `main` |
| Authoritative path | `protocol/protocol.yaml` |
| Protocol name | `host-device-control-poc` |
| Protocol version | `0.1.0` |
| Wire version | `0x01` |
| Contract status | `candidate_for_alignment` |
| Authority rule | `specification_precedes_implementation` |
| Snapshot SHA-256 | `7ff8db3a1ed669407e0d4cada2a78b212ea3c7bccdf371f232a2689a02e7c56e` |
| MCU implementation baseline | `jkman357/host-device-control-poc-stm32f446re-fw` commit `4b1b701` |
| Snapshot captured | 2026-07-25 |

## Repository rule

The YAML in `Protocol/Spec/Host_Device_Control_PoC_protocol.yaml` is a pinned implementation snapshot, not a new
MCU-owned authority. When this snapshot and the system repository differ, implementation work stops until the
system-repository change is reviewed, pinned, and intentionally propagated.

The MCU repository may add implementation notes and tests, but it shall not independently change wire framing,
Message IDs, payload fields, result codes, state semantics, timeouts, or compatibility interpretation.

## Lifecycle boundary

This revision implements the MCU side against the `candidate_for_alignment` contract. It does not change the
contract status. Promotion requires all evidence listed by the authoritative lifecycle rule:

1. Matching PC and MCU implementations.
2. Passing shared byte-level test vectors.
3. Hardware interoperability evidence.
4. Pinned compatible commits.
5. Human approval recorded in the system repository.

## Intentionally unimplemented emissions

`DEVICE_STATUS` and `ERROR_REPORT` remain defined in the contract and in MCU Message ID constants. This MCU
revision does not emit them because the current contract does not define:

- a request or event trigger for `DEVICE_STATUS`;
- an error-code allocation and emission policy for `ERROR_REPORT`.

No implementation-specific semantics are introduced. Those behaviors require a contract-first change.
