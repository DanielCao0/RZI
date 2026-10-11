# West patches

Status: implemented

RZI owns the temporary compatibility patches required by its pinned backend
dependencies. Applications import RZI through `west.yml` and do not copy
these files.

See also: [boot.md](./boot.md).

The patch definition is stored in:

```text
rzi/
└── zephyr/
    ├── patches.yml
    └── patches/
        └── usp_zephyr/
            ├── 0005-stm32wl-subghz-radio.patch
            └── 0006-zephyr-4.5-warning-fixes.patch
        └── usp/
            ├── 0001-export-secure-element-session-keys.patch
            ├── 0002-ral-sx126x-frequency-limits.patch
            └── 0003-user-channel-mask.patch
```

Pins are USP v1.2.1 and Zephyr 4.5.0-rc1. v1.2.1 already contains the former
Zephyr 4.4 warning fixes, the LR-FHSS source path, removal of the duplicate
`xiao_nrf54l15` board, and the global SX1262 compile definition.
`0005` teaches the USP SX126x driver the STM32WL on-chip SUBGHZ radio used by
RAK3372. `0006` replaces the deprecated `<zephyr/sys_clock.h>` include and
`SPI_OP_MODE_MASTER` on Zephyr 4.5.0-rc1. `usp/0001` exports session keys
from the software secure element. The LR11xx crypto engine cannot export key
material and returns an error. `usp/0002` turns the SX126x frequency-limit
redefinition in `ral_sx126x.h` into `#if`/`#else`; upstream's redefinition is
a warning that twister's `-Werror` turns into a build failure on every SX1262
board. `usp/0003` adds `smtc_modem_set_channel_mask()` and
`smtc_modem_get_channel_mask()`. The upstream API has no way to limit the
US915, AU915 or CN470 channels before join. The patch ANDs the user mask into
every default channel mask (join, the post-join defaults, and the CN470
session plan). It also makes the US915/AU915 channel rotation ignore disabled
channels; otherwise a mask stalls the rotation. A network LinkADRReq and the
ADR back-off still replace the mask until the next join, as LoRaWAN requires.

## Applying the patches

Run the commands from the west workspace root after `west update`:

```sh
west patch -sm rzi clean
west patch -sm rzi apply --roll-back
```

`-sm rzi` (`--src-module rzi`) tells `west patch` to read the patch definition
from the RZI module. It does not mean that the patches target RZI. Each entry's
`module` field in `patches.yml` selects the destination module, currently
`usp_zephyr` or `usp`.

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
