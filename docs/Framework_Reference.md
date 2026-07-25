# Framework Reference

## Source identity

| Field | Value |
|---|---|
| Framework repository | `jkman357/host-device-control-framework` |
| Authoritative branch | `main` |
| Exact source commit | `7a68980ef5faa2e897a3574af121683d65f74638` |
| Reference captured | 2026-07-25 |
| Package status | Draft PoC baseline |

## Applied document baselines

The package was structured against the current document identities observed on the framework `main` branch on 2026-07-25:

| Document | Referenced version |
|---|---:|
| Coordinator Node Control Framework | v1.1.5 |
| Protocol YAML Definition Guide | v1.1.0 |
| Protocol YAML Template | v1.1.0 |
| Node Software Engineering Rules | v1.1.0 |
| Embedded C Coding Rules | v1.0.17 |

The commit above is the immutable framework source identity used for this generated package. Before adoption, a human shall verify that the intended reference is still this commit or deliberately update the analysis to a newer commit.

## Authority separation

The framework defines engineering method, role boundaries, review criteria, and Protocol-document structure. It does not own this PoC's concrete wire values. The concrete project-level wire authority is `host-device-control-poc-system/protocol/protocol.yaml`; the MCU and PC repositories are consumers of that contract.

## Application boundary

This repository applies only the portions needed for a single-node, connection-bound, UART PoC:

- Device-side event handling and state ownership.
- Command/response and telemetry message separation.
- Static resource bounds and explicit overflow diagnostics.
- Protocol IDs, framing, CRC, sequence, and compatibility metadata.
- Human approval and evidence boundaries.

Not claimed or implemented:

- Multi-node routing or discovery.
- Secure session establishment, authentication, or encryption.
- Firmware update.
- Product safety, cybersecurity, regulatory, or production qualification.
- Automatic source generation from the Protocol YAML.
