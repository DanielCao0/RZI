# Documentation

Status: index

In-tree copies are English. File names are lowercase kebab-case and do not
repeat `rzi`.

Every page uses the same front matter: a short title, a `Status:` line, a
purpose paragraph, and `See also:` links to peer documents. Status values:

| Status | Meaning |
|---|---|
| index | This catalog |
| normative | Rules new code must follow |
| implemented | Describes current source |
| evaluation | Findings from a second implementation |
| superseded | Kept only so old links resolve |

`include/rzi/` is the ABI. These Markdown files explain ownership, build
switches, and limits that do not belong in a header comment.

## Architecture and boot

| Document | Status | Contents |
|---|---|---|
| [architecture.md](./architecture.md) | implemented | Layers, ownership, backend policy |
| [boot.md](./boot.md) | implemented | Product boards, MCUBoot, slots, merged image |
| [power.md](./power.md) | implemented | Policy, blockers, both boards |
| [west-patch.md](./west-patch.md) | implemented | Upstream compatibility patches |

## Service APIs

| Document | Status | Contents |
|---|---|---|
| [error-codes.md](./error-codes.md) | normative | Closed `RZI_ERR_*` catalog |
| [lorawan-api.md](./lorawan-api.md) | normative | LoRaWAN public C contract |
| [lora-api.md](./lora-api.md) | implemented | Raw LoRa / FSK P2P C contract |
| [lorawan-backends.md](./lorawan-backends.md) | implemented | USP vs Zephyr build switch |
| [lorawan-backend-zephyr.md](./lorawan-backend-zephyr.md) | evaluation | Zephyr `lorawan_*` adapter |
| [fuota.md](./fuota.md) | implemented | ChirpStack FUOTA (dual-slot) |
| [storage-api.md](./storage-api.md) | implemented | Namespaced key-value storage |
| [power-api.md](./power-api.md) | implemented | `rzi_power_*` C contract |
| [at-framework.md](./at-framework.md) | implemented | AT framework, registry, adapters |
| [at-command-compatibility.md](./at-command-compatibility.md) | implemented | Registered commands vs RUI3 |
| [rui3-mapping.md](./rui3-mapping.md) | implemented | Public C ABI mapped from RUI3, plus C-level gaps |
| [rui3-gap.md](./rui3-gap.md) | superseded | Redirect to rui3-mapping and AT compatibility |

## Standards

| Document | Status | Contents |
|---|---|---|
| [coding-standards.md](./coding-standards.md) | normative | Layout, naming, errors, commits |
| [api-docs.md](./api-docs.md) | normative | Doxygen and compiler annotations |
| [sbom.md](./sbom.md) | implemented | Module SPDX / CycloneDX SBOM |
| [ci.md](./ci.md) | implemented | CI jobs, version pins, local reproduction |
| [naming-conventions.md](./naming-conventions.md) | superseded | Redirect to coding standards §3 |

## Doxygen

```bash
scripts/generate-doxygen.sh
```

Opens `doc/doxygen/html/index.html`. Generated HTML is not committed.
Rules are in [api-docs.md](./api-docs.md).
