#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Generate a small ChirpStack FUOTA test payload for the RZI sample."""

from pathlib import Path

SIZE = 4096
MAGIC = b"RZI1"
MESSAGE = b"ChirpStack FUOTA test payload for RZI\n"


def main() -> None:
    payload = bytearray(SIZE)
    payload[0:4] = MAGIC
    payload[4:8] = SIZE.to_bytes(4, "little")
    payload[8 : 8 + len(MESSAGE)] = MESSAGE
    for index in range(8 + len(MESSAGE), SIZE):
        payload[index] = index & 0xFF

    output = Path(__file__).with_name("rzi-fuota-test.bin")
    output.write_bytes(payload)
    print(f"wrote {output} ({SIZE} bytes)")


if __name__ == "__main__":
    main()
