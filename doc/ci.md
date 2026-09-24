# CI

Status: implemented

GitHub Actions gates every push to `main` and every pull request. Compliance
follows Zephyr's `scripts/ci/check_compliance.py` workflow. Tests and samples
follow the `example-application` twister invocation (`--integration`).
Public-header, documentation, and SBOM checks are RZI gates. Pull requests
also run the Developer Certificate of Origin check.

See also: [coding-standards.md](./coding-standards.md),
[architecture.md](./architecture.md), [west-patch.md](./west-patch.md).

## Workflow

`.github/workflows/ci.yml` runs six jobs in parallel. A composite action,
`.github/actions/setup-workspace`, builds the west workspace for the jobs
that need one.

| Job | What it proves |
|---|---|
| Compliance | `scripts/check-compliance.sh` (Zephyr `check_compliance.py`), `scripts/check-style.sh --no-checkpatch` (clang-format, `@file` / `@brief`), no backend symbols in `include/rzi/`, ASCII-only source, SPDX tags, version strings in sync |
| DCO | `Signed-off-by` on every pull-request commit (`.github/workflows/dco.yml`) |
| Twister (native_sim) | `west twister -T rzi/tests --integration` on the host |
| Samples (product boards) | `west twister -T rzi/samples --integration` for `rzi_rak4631` and `rzi_rak3372` with the Zephyr SDK |
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

# Zephyr check_compliance.py. ZEPHYR_BASE may be omitted when this repo
# sits in a west workspace.
scripts/check-compliance.sh origin/main..HEAD

# Tests and samples, from the west workspace topdir
west twister -T rzi/tests --inline-logs -v --integration
west twister -T rzi/samples --inline-logs -v --integration

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
