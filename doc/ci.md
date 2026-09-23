# CI

Status: implemented

GitHub Actions gates every push to `main` and every pull request, following
the Zephyr project CI model: compliance first, then twister, then sample
builds, plus public-header, documentation, and SBOM checks.

See also: [coding-standards.md](./coding-standards.md),
[architecture.md](./architecture.md), [west-patch.md](./west-patch.md).

## Workflow

`.github/workflows/ci.yml` runs six jobs in parallel. A composite action,
`.github/actions/setup-workspace`, builds the west workspace for the jobs
that need one.

| Job | What it proves |
|---|---|
| Compliance | `scripts/check-style.sh` (clang-format, `@file` / `@brief`, checkpatch), no backend symbols in `include/rzi/`, ASCII-only source, SPDX tags, version strings in sync |
| Twister (native_sim) | `west twister -T rzi/tests` on the host |
| Samples (product boards) | `west twister -T rzi/samples` for `rzi_rak4631` and `rzi_rak3372` with the Zephyr SDK |
| Public headers | every `include/rzi/**.h` compiles standalone as C11 and as C++17 |
| Documentation | `scripts/generate-doxygen.sh` with zero warnings |
| SBOM | `scripts/generate-sbom.sh` regenerates valid SPDX and CycloneDX JSON |

Failed twister jobs upload `twister-out/` as a build artifact.

## Workspace construction

RZI is a module, so CI builds the workspace a customer would:

1. A temporary manifest is generated: Zephyr at `ZEPHYR_REVISION` plus the
   `usp_zephyr` / `usp` revisions read from `west.yml`, all with
   `clone-depth: 1`. The checked-out RZI tree is the code under test; it is
   not cloned again.
2. `west update --narrow` fetches the pinned revisions. GitHub serves
   reachable commit SHAs, so shallow fetches of pinned commits work.
3. The patches under `zephyr/patches/usp_zephyr/` are applied with
   `git apply` (idempotent: already-applied patches are skipped). This
   mirrors `west patch -sm rzi apply` without making RZI a west project of
   the CI manifest.
4. `ZEPHYR_EXTRA_MODULES` points at the checkout, so builds and twister see
   RZI exactly as committed.

The workspace and the Zephyr SDK are cached between runs; the cache keys
include the Zephyr revision and `west.yml` hash, so a version bump starts
from a cold cache automatically.

## Version pins

| Pin | Where | Bump policy |
|---|---|---|
| `ZEPHYR_REVISION` | `ci.yml` env | Together with `zephyr/patches/`; drop patches the new revision already contains |
| `ZEPHYR_SDK_VERSION` | `ci.yml` env | Match the SDK required by the Zephyr revision |
| `CLANG_FORMAT_VERSION` | `ci.yml` env | Same release developers run locally |
| `DOXYGEN_VERSION` | `ci.yml` env | Same release developers run locally |

`usp_zephyr` and `usp` revisions come from `west.yml`; CI never carries a
second copy.

## Local reproduction

Prerequisites match the runner: a Python environment with
`zephyr/scripts/requirements-{base,build-test,run-test}.txt` installed,
`device-tree-compiler`, `gperf`, and `gcc-multilib` / `g++-multilib` for the
32-bit `native_sim/native` variant. Sample builds need the Zephyr SDK
(`ZEPHYR_SDK_INSTALL_DIR`).

```bash
# Compliance (uses the west workspace Zephyr for checkpatch)
scripts/check-style.sh

# Tests and samples, from the west workspace topdir
west twister -T rzi/tests --inline-logs -v
west twister -T rzi/samples --inline-logs -v

# Public headers against a configured build
west build -b native_sim/native/64 rzi/tests/lorawan/service -d build-headers
python3 scripts/check-public-headers.py build-headers

# Documentation and SBOM
scripts/generate-doxygen.sh
scripts/generate-sbom.sh
```

On a host without the Zephyr SDK, the native_sim jobs also work with
`ZEPHYR_TOOLCHAIN_VARIANT=host`, which skips the SDK lookup that
`verify-toolchain.cmake` performs.

## Out of scope

CI proves host-side and build-time behavior. RF verification on RAK4631 /
RAK3372 hardware (OTAA, RX windows, sleep current, FUOTA recovery) remains a
release gate; see [architecture.md](./architecture.md) section 16.
