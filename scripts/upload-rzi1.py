#!/usr/bin/env python3
# Copyright (c) 2026 RAKwireless
# SPDX-License-Identifier: Apache-2.0
"""Upload a sketch or signed slot1 image over CDC (RZI1).

Type 0 (sketch) is handled by the Arduino loader.
Type 1 (signed slot) is the shared contract with RZI rzi_slot_update.
"""

from __future__ import annotations

import argparse
import sys
import time
import zlib

try:
    import serial
    from serial.tools import list_ports
except ImportError:
    sys.stderr.write("error: pyserial is required (pip3 install pyserial)\n")
    sys.exit(2)

MAGIC = b"RZI1"
TYPE_SKETCH = 0
TYPE_SLOT1 = 1
CHUNK = 1024
IMAGE_MAGIC = 0x96F3B83D


def crc32(data: bytes) -> int:
    return zlib.crc32(data) & 0xFFFFFFFF


def pack_header(kind: int, payload: bytes) -> bytes:
    return (
        MAGIC
        + bytes((kind, 0, 0, 0))
        + len(payload).to_bytes(4, "little")
        + crc32(payload).to_bytes(4, "little")
    )


def parse_id(value: str | None) -> int | None:
    if value is None or value == "" or value.startswith("{"):
        return None
    return int(value, 0)


def list_devices() -> list[str]:
    return [p.device for p in list_ports.comports()]


def ports_by_vid_pid(vid: int | None, pid: int | None) -> list[str]:
    if vid is None:
        return []
    found = []
    for p in list_ports.comports():
        if p.vid != vid:
            continue
        if pid is not None and p.pid != pid:
            continue
        found.append(p.device)
    return found


def resolve_port(preferred: str, vid: int | None, pid: int | None) -> str:
    now = list_devices()
    if preferred in now:
        return preferred
    match = ports_by_vid_pid(vid, pid)
    if match:
        return match[0]
    raise TimeoutError(f"no upload port (last seen {preferred})")


def touch_1200(port: str) -> None:
    s = serial.Serial(port, baudrate=1200, timeout=0.2)
    time.sleep(0.3)
    s.close()


def wait_port_change(
    preferred: str,
    vid: int | None,
    pid: int | None,
    timeout: float,
    wait_disconnect: bool,
) -> str:
    before = set(list_devices())
    deadline = time.time() + timeout
    gone = not wait_disconnect
    while time.time() < deadline:
        now = set(list_devices())
        if wait_disconnect and not gone:
            if preferred not in now or (vid is not None and not ports_by_vid_pid(vid, pid)):
                gone = True
            time.sleep(0.2)
            continue
        try:
            return resolve_port(preferred, vid, pid)
        except TimeoutError:
            time.sleep(0.2)
    if gone:
        raise TimeoutError(f"device did not re-enumerate within {timeout:.0f}s")
    # Never disconnected — already in update mode.
    return resolve_port(preferred, vid, pid)


def wait_token(ser: serial.Serial, token: bytes, timeout: float) -> bytes:
    deadline = time.time() + timeout
    buf = b""
    ser.timeout = 0.2
    while time.time() < deadline:
        chunk = ser.read(64)
        if chunk:
            buf += chunk
            if token in buf:
                return buf
            if b"ERR " in buf:
                raise RuntimeError(buf.decode("ascii", "replace").strip())
        time.sleep(0.02)
    raise TimeoutError(f"timed out waiting for {token!r}, got {buf!r}")


def open_update_port(port: str, vid: int | None, pid: int | None, do_touch: bool) -> serial.Serial:
    if do_touch:
        try:
            touch_1200(port)
        except serial.SerialException:
            pass
        time.sleep(0.5)
        port = wait_port_change(port, vid, pid, 15.0, wait_disconnect=True)
        time.sleep(0.4)

    port = resolve_port(port, vid, pid)
    ser = serial.Serial(port, baudrate=115200, timeout=1)
    ser.dtr = True
    ser.rts = False
    time.sleep(0.3)
    ser.reset_input_buffer()
    try:
        wait_token(ser, b"RZI1-RDY", 8.0)
        return ser
    except TimeoutError:
        ser.close()
        raise


def upload(port: str, path: str, kind: int, touch: bool, vid: int | None, pid: int | None, wait_reboot: float) -> None:
    with open(path, "rb") as f:
        payload = f.read()
    if not payload:
        raise SystemExit(f"error: empty file {path}")

    if kind == TYPE_SLOT1:
        magic = int.from_bytes(payload[:4], "little")
        if magic != IMAGE_MAGIC:
            raise SystemExit(
                "error: slot1 needs a signed MCUboot image (.signed.bin), "
                f"not {path} (magic 0x{magic:08x})"
            )

    header = pack_header(kind, payload)
    print(f"file {path}  {len(payload)} bytes  crc=0x{crc32(payload):08x}  type={kind}")

    ser = None
    last_err: Exception | None = None
    attempts = [False, True] if touch else [False]

    for do_touch in attempts:
        try:
            if do_touch:
                print(f"1200-bps reset on {port}")
            ser = open_update_port(port, vid, pid, do_touch)
            port = ser.port
            break
        except (TimeoutError, serial.SerialException) as exc:
            last_err = exc
            ser = None
    if ser is None:
        raise SystemExit(f"error: loader did not advertise RZI1-RDY ({last_err})")

    try:
        ser.write(header)
        ser.flush()
        wait_token(ser, b"RZI1-GO", 30.0)
        print("erased, sending...")
        sent = 0
        while sent < len(payload):
            n = ser.write(payload[sent : sent + CHUNK])
            sent += n
            if sent == len(payload) or sent % (32 * 1024) < CHUNK:
                print(f"  {sent}/{len(payload)}", flush=True)
        ser.flush()
        reply = wait_token(ser, b"OK", 30.0)
        if b"ERR " in reply:
            raise RuntimeError(reply.decode("ascii", "replace"))
        print("OK")
    finally:
        ser.close()

    if wait_reboot > 0:
        print(f"waiting for loader reboot ({wait_reboot:.0f}s)...")
        port = wait_port_change(port, vid, pid, wait_reboot, wait_disconnect=True)
        print(f"loader back on {port}")


def main() -> int:
    p = argparse.ArgumentParser(description="USB CDC upload for the Arduino+RZI loader")
    p.add_argument("--port", required=True, help="serial port, e.g. /dev/ttyACM0 or COM5")
    p.add_argument("--type", choices=("sketch", "slot1"), default="sketch")
    p.add_argument("--no-touch", action="store_true", help="do not try a 1200-bps reset")
    p.add_argument("--vid", default="", help="USB VID for port rediscovery (e.g. 0x2341)")
    p.add_argument("--pid", default="", help="USB PID for port rediscovery (e.g. 0x0001)")
    p.add_argument(
        "--wait-reboot",
        type=float,
        default=0,
        help="after OK, wait this many seconds for the board to re-enumerate",
    )
    p.add_argument("file", help="sketch .bin / .elf-zsk.bin, or signed slot1 image")
    args = p.parse_args()
    kind = TYPE_SLOT1 if args.type == "slot1" else TYPE_SKETCH
    upload(
        args.port,
        args.file,
        kind,
        touch=not args.no_touch,
        vid=parse_id(args.vid),
        pid=parse_id(args.pid),
        wait_reboot=args.wait_reboot,
    )
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except KeyboardInterrupt:
        raise SystemExit(130)
    except Exception as exc:  # noqa: BLE001 — CLI tool
        sys.stderr.write(f"error: {exc}\n")
        raise SystemExit(1)
