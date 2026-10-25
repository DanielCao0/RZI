# CI

Status: implemented

GitHub Actions gates every push to `main` and every pull request. Compliance
follows Zephyr's `scripts/ci/check_compliance.py` workflow. Tests and samples
follow the `example-application` twister invocation (`--integration`).
Public-header, documentation, and SBOM checks are RZI gates. Pull requests
also run the Developer Certificate of Origin check.

See also: [coding-standards.md](./coding-standards.md),
[architecture.md](./architecture.md), [west-patch.md](./west-patch.md),
[sbom.md](./sbom.md).

## Workflow

`.github/workflows/ci.yml` runs six jobs in parallel. Composite actions
`.github/actions/setup-workspace` and `.github/actions/setup-sdk` build the
west workspace and install the Zephyr SDK for the jobs that need them.

| Job | What it proves |
|---|---|
| Compliance | `scripts/check-compliance.sh` (Zephyr `check_compliance.py`), `scripts/check-style.sh --no-checkpatch` (clang-format, `@file` / `@brief`), no backend symbols in `include/rzi/`, ASCII-only source, `reuse lint`, version strings in sync (`version.h`, `Doxyfile`, `module.yml` CPE / PURL) |
| DCO | `Signed-off-by` on every pull-request commit (`.github/workflows/dco.yml`) |
| Twister (native_sim) | `west twister -T rzi/tests --integration` on the host |
| Samples (product boards) | `west twister -T rzi/samples --integration` for `rak4631/nrf52840/rzi`, `rak3172/stm32wle5xx/rzi` and `rak3362` with the Zephyr SDK; then `samples/at` on RAK4631 with build metadata, an SPDX 3 firmware SBOM, `spdx3-validate`, and `check-sbom.py --strict` |
| Public headers | every `include/rzi/**.h` compiles standalone as C11 and as C++17 |
| Documentation | `scripts/generate-doxygen.sh` with zero warnings |
| SBOM | `scripts/generate-sbom.sh`, `spdx3-validate`, CycloneDX 1.6 schema, and `check-sbom.py --strict` on the module SBOM |

Failed twister jobs upload `twister-out/` as a build artifact. The SBOM and
Samples jobs upload the module and firmware SBOMs.

`.github/workflows/release.yml` runs on a `v<version>` tag and publishes the
release images, their SBOMs, the module SBOM, and checksums; see
[sbom.md](./sbom.md#releases).

## Workspace construction

RZI is a module, so CI builds the workspace a customer would:

1. A temporary manifest is generated from the `zephyr`, `usp_zephyr`, and
   `usp` projects in `west.yml`. The checked-out RZI tree is the code under
   test; it is not cloned again.
2. `west update --narrow -o=--filter=blob:none` fetches those revisions
   without a depth-1 clone. A shallow fetch of a pinned SHA is an unadvertised
   object and git rejects it.
3. `zephyr`, `usp_zephyr`, and `usp` are each reset to the revision in
   `west.yml`. The workspace cache otherwise keeps the previous run's
   patched tree, and the next patch then fails its context check. The
   patches under `zephyr/patches/zephyr/`, `zephyr/patches/usp_zephyr/`,
   and `zephyr/patches/usp/` are applied with `git apply`. This mirrors
   `west patch -sm rzi apply` without making RZI a west project of the
   CI manifest.
4. `ZEPHYR_EXTRA_MODULES` points at the checkout, so builds and twister see
   RZI exactly as committed.

The workspace and the Zephyr SDK are cached between runs; the workspace cache
key is the `west.yml` hash, so a version bump starts from a cold cache
automatically.

## Version pins

| Pin | Where | Bump policy |
|---|---|---|
| Zephyr revision | `west.yml` | Together with `zephyr/patches/`; drop patches the new revision already contains |
| `usp_zephyr` / `usp` revisions | `west.yml` | Together with `zephyr/patches/` |
| `ZEPHYR_SDK_VERSION` | `ci.yml` env | Match the SDK required by the Zephyr revision |
| `CLANG_FORMAT_VERSION` | `ci.yml` env | Same release developers run locally |
| `DOXYGEN_VERSION` | `ci.yml` env | Same release developers run locally |

`west.yml` is the only copy of the Zephyr and USP revisions; CI reads them
from there.

## Compliance scope

`scripts/check-compliance.sh` runs Zephyr's `check_compliance.py` with
`ZEPHYR_BASE` set, on the same commit range Zephyr uses (`origin/<base>..HEAD`
for a pull request). Pull requests are rebased onto the target branch, and a
merge commit fails the job.

The script loads Zephyr's `.checkpatch.conf` and `.gitlint`, rewriting paths
so checkpatch's typedefs file and gitlint's commit rules come from the Zephyr
tree. Two local ignores stay in place. `EXECUTE_PERMISSIONS`: hook and helper
scripts must keep the executable bit. Gitlint rule `UC2`: it requires two
whitespace-separated name tokens, while the DCO check accepts the committer
name as configured, including a single token.

These checks are excluded because they validate the Zephyr repository, not a
module:

- `Kconfig`, `KconfigBasic`, `KconfigBasicNoModules`, `KconfigHWMv2`
- `SysbuildKconfig`, `SysbuildKconfigBasic`, `SysbuildKconfigBasicNoModules`
- `ZephyrModuleFile`

`ClangFormat` is excluded the same way as Zephyr's compliance workflow.
`scripts/check-style.sh` still fails the job when clang-format or a missing
`@file` / `@brief` is wrong. `LicenseAndCopyrightCheck` stays a warning, as
in that workflow. Every other report fails the job.

`native_sim` keeps `ZEPHYR_TOOLCHAIN_VARIANT=host`. The SDK install in CI is
the ARM toolchain the product boards need; the host compiler builds the
simulator.

## Local reproduction

Prerequisites match the runner: a Python environment with
`zephyr/scripts/requirements-{base,build-test,run-test}.txt` installed,
`device-tree-compiler`, `gperf`, and `gcc-multilib` / `g++-multilib` for the
32-bit `native_sim/native` variant. Sample builds need the Zephyr SDK
(`ZEPHYR_SDK_INSTALL_DIR`). Compliance also needs
`zephyr/scripts/requirements-actions.txt` and `libmagic1`.

```bash
# clang-format, @file / @brief, and checkpatch
scripts/check-style.sh

# Zephyr gitlint on one message. .githooks/commit-msg runs this.
scripts/check-commit-msg.sh .git/COMMIT_EDITMSG

# Zephyr check_compliance.py. ZEPHYR_BASE may be omitted when this repo
# sits in a west workspace.
scripts/check-compliance.sh origin/main..HEAD

# Tests and samples, from the west workspace topdir
west twister -T rzi/tests --inline-logs -v --integration
west twister -T rzi/samples --inline-logs -v --integration

# Public headers against a configured build
west build -b native_sim/native/64 rzi/tests/lorawan/service -d build-headers
python3 scripts/check-public-headers.py build-headers

# Documentation, REUSE, and SBOM
scripts/generate-doxygen.sh
reuse lint
scripts/generate-sbom.sh --allow-dirty
spdx3-validate -j build/sbom/rzi.spdx.jsonld
python3 scripts/check-sbom.py --strict build/sbom/rzi.spdx.jsonld build/sbom/rzi.cdx.json
```

Firmware SBOM steps are in [sbom.md](./sbom.md#firmware-sbom).
`spdx3-validate` downloads the SPDX 3 context and SHACL model, so it needs
network access.

On a host without the Zephyr SDK, the native_sim jobs also work with
`ZEPHYR_TOOLCHAIN_VARIANT=host`, which skips the SDK lookup that
`verify-toolchain.cmake` performs.

## Out of scope

CI proves host-side and build-time behavior. RF verification on RAK4631 /
RAK3372 hardware (OTAA, RX windows, sleep current, FUOTA recovery) remains a
release gate; see [architecture.md](./architecture.md) section 16.
