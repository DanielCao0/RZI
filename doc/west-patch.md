# RZI west patches

RZI owns the temporary compatibility patches required by its pinned backend
dependencies. Applications import RZI and its dependencies through `west.yml`;
they do not need to copy or maintain these patch files.

The patch definition is stored in:

```text
rzi/
└── zephyr/
    ├── patches.yml
    └── patches/
        └── usp_zephyr/
            ├── 0001-zephyr-4.4-warning-fixes.patch
            ├── 0002-fix-lr-fhss-src-path.patch
            ├── 0003-disable-duplicate-xiao-board-root.patch
            └── 0004-sx1262-pa-compile-definitions.patch
```

The third patch stops `usp_zephyr` from exporting its historical
`xiao_nrf54l15` board root because the pinned Zephyr 4.4 revision already owns
that board. USP devicetree bindings, shields, and module extensions remain
exported.

## Applying the patches

Run the commands from the west workspace root after `west update`:

```sh
west patch -sm rzi clean
west patch -sm rzi apply --roll-back
```

`-sm rzi` (`--src-module rzi`) tells `west patch` to read the patch definition
from the RZI module. It does not mean that the patches target RZI. Each entry's
`module` field in `patches.yml` selects the destination module, currently
`usp_zephyr`.

`apply` is not idempotent, so automated build wrappers should run `clean`
before `apply`. `--roll-back` cleans modules already modified by the current
operation if a later patch fails.

Warning: `clean` runs the configured `checkout-command`, currently
`git checkout .`, in every destination module. It discards uncommitted tracked
changes in those modules. `clean-command` is deliberately empty so that
untracked files are preserved.

## Maintaining the manifest

After adding or modifying a patch, regenerate the manifest from the RZI
repository root:

```sh
python3 scripts/generate-patch-manifest.py
```

The generator computes checksums using the same normalized-text algorithm as
Zephyr's `west patch`; this avoids CRLF-related checksum differences.

Patches are temporary compatibility measures. Remove a patch once the pinned
upstream revision contains the equivalent fix, and prefer immutable upstream
commits for RZI releases.
