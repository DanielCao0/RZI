#!/usr/bin/env python3
# Copyright (c) 2026 RAKwireless
# SPDX-License-Identifier: Apache-2.0
"""Compile every public RZI header standalone, as C and as C++.

Usage:

    scripts/check-public-headers.py <build-dir>

<build-dir> is a configured Zephyr build of an RZI sample (it must contain
compile_commands.json). The compile flags of that build - include paths,
defines, and the generated autoconf.h - make <zephyr/...> resolvable while
each <rzi/...> header is compiled on its own. A header that only compiles
because some other header was included first fails here.
"""
from __future__ import annotations

import json
import shlex
import subprocess
import sys
import tempfile
from pathlib import Path

# Flags that make includes and macros resolvable. Everything else
# (optimisation, warnings, codegen) is irrelevant for a syntax-only probe.
SEPARATE_FLAGS = ("-I", "-isystem", "-D", "-imacros", "-include")
JOINED_PREFIXES = ("-I", "-D", "-isystem", "-imacros", "-include")

PROBES = (
    ("c", "gcc", "c11"),
    ("c++", "g++", "gnu++17"),
)


def collect_flags(build: Path) -> list[str]:
    """Union the include/define flags of every translation unit."""
    ccdb_path = build / "compile_commands.json"
    if not ccdb_path.is_file():
        raise SystemExit(f"{ccdb_path} missing; configure a sample build first")

    flags: list[str] = []
    seen: set[str] = set()
    for entry in json.loads(ccdb_path.read_text()):
        args = entry.get("arguments") or shlex.split(entry["command"])
        i = 0
        while i < len(args):
            arg = args[i]
            if arg in SEPARATE_FLAGS and i + 1 < len(args):
                key = f"{arg} {args[i + 1]}"
                if key not in seen:
                    seen.add(key)
                    flags.extend((arg, args[i + 1]))
                i += 2
                continue
            if arg.startswith(JOINED_PREFIXES) and arg not in seen:
                seen.add(arg)
                flags.append(arg)
            i += 1
    return flags


def main() -> int:
    if len(sys.argv) != 2:
        raise SystemExit(__doc__)

    build = Path(sys.argv[1]).resolve()
    repo = Path(__file__).resolve().parent.parent
    flags = collect_flags(build)

    headers = sorted((repo / "include" / "rzi").rglob("*.h"))
    if not headers:
        raise SystemExit("no public headers found under include/rzi")

    failures: list[tuple[Path, str, str]] = []
    with tempfile.TemporaryDirectory() as tmp:
        probe_c = Path(tmp) / "probe.c"
        probe_cpp = Path(tmp) / "probe.cpp"
        for header in headers:
            include = header.relative_to(repo / "include")
            for lang, compiler, std in PROBES:
                probe = probe_c if lang == "c" else probe_cpp
                probe.write_text(f"#include <{include}>\n")
                cmd = [compiler, f"-std={std}", "-fsyntax-only", *flags, str(probe)]
                result = subprocess.run(cmd, capture_output=True, text=True)
                if result.returncode != 0:
                    failures.append((include, lang, result.stderr.strip()))

    for include, lang, stderr in failures:
        print(f"{include}: does not compile standalone as {lang.upper()}")
        print(stderr)
        print()
    if failures:
        print(f"public headers: {len(failures)} failure(s) across {len(headers)} headers")
        return 1
    print(f"public headers: {len(headers)} headers compile as C and C++")
    return 0


if __name__ == "__main__":
    sys.exit(main())
