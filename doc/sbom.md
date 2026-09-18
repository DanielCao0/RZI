# SBOM

Status: implemented

Source-level software bill of materials for the RZI Zephyr module.

See also: [west-patch.md](./west-patch.md).

RZI publishes a **module SBOM**: this repository plus the backend projects
pinned in `west.yml`. That is what a customer imports. It is not a firmware
image SBOM. Zephyr, MCUBoot, Nordic/STM32 HALs, and compiler libraries are
chosen by the application workspace and only appear after a product build.

## Generated files

```bash
scripts/generate-sbom.sh
```

| File | Format | Typical consumer |
|---|---|---|
| [sbom/rzi.spdx.json](./sbom/rzi.spdx.json) | SPDX 2.3 JSON | License review, NTIA minimum elements |
| [sbom/rzi.cdx.json](./sbom/rzi.cdx.json) | CycloneDX 1.6 JSON | Dependency-Track, Grype, other vuln tools |

The generator reads `include/rzi/version.h`, `git rev-parse HEAD`, `west.yml`,
and `zephyr/patches.yml`. Re-run it when the SDK version or a pinned revision
changes.

Recorded components today:

| Component | License | Pinned by |
|---|---|---|
| `rzi` | Apache-2.0 | this repository |
| `usp_zephyr` | BSD-3-Clause-Clear | `west.yml` (RZI west patches applied) |
| `usp` | BSD-3-Clause-Clear | `west.yml` |

## Firmware image SBOM

After building a product or sample image, use Zephyr's SPDX exporter so the
document includes the files that actually linked:

```sh
west spdx --init -d build/app
west build -d build/app --sysbuild -b rzi_rak4631/nrf52840 -- \
    -DCONFIG_BUILD_OUTPUT_META=y
west spdx -d build/app -n rzi
```

`build/app/spdx/` then contains `app.spdx`, `zephyr.spdx`, `build.spdx`, and
`modules-deps.spdx`. Those documents belong with the release artifacts, not
in this module tree.
