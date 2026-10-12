#!/usr/bin/env python3
# SPDX-FileCopyrightText: Copyright (c) 2026 RAKwireless
# SPDX-License-Identifier: Apache-2.0
"""Write SPDX 3 SBOMs for every image of a Zephyr build.

Usage:

  scripts/generate-firmware-sbom.py BUILD_DIR [-o OUT_DIR]

BUILD_DIR is a plain or sysbuild build directory. Every image must be built
with CONFIG_BUILD_OUTPUT_META=y. For sysbuild, pass it to each image:

  west build -b rzi_rak4631/nrf52840 --sysbuild samples/at -d build/at -- \\
      -DCONFIG_BUILD_OUTPUT_META=y -Dmcuboot_CONFIG_BUILD_OUTPUT_META=y

Output: OUT_DIR/<image>/{app,zephyr,build,modules-deps}.jsonld from west
spdx, plus image.jsonld: a build-type software_Sbom whose root elements are
the image files that get flashed, with SHA-256 and SHA-512 hashes, generated
from the zephyr.elf in build.jsonld. Default OUT_DIR is BUILD_DIR/spdx.
Run inside the west workspace.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import subprocess
import sys
import uuid
from datetime import UTC, datetime
from pathlib import Path

import yaml

NAMESPACE_ROOT = "https://github.com/DanielCao0/RZI/spdx/firmware"
IMAGE_FILES = ("zephyr.hex", "zephyr.bin", "zephyr.signed.hex", "zephyr.signed.bin")


def images(build_dir: Path) -> dict[str, Path]:
    domains = build_dir / "domains.yaml"
    if not domains.is_file():
        return {build_dir.name: build_dir}
    data = yaml.safe_load(domains.read_text("utf-8"))
    return {d["name"]: Path(d["build_dir"]) for d in data["domains"]}


def has_build_meta(image_dir: Path) -> bool:
    config = image_dir / "zephyr" / ".config"
    return config.is_file() and "CONFIG_BUILD_OUTPUT_META=y\n" in config.read_text("utf-8")


def elf_spdx_id(build_doc: Path) -> str:
    """Return the expanded spdxId of zephyr/zephyr.elf in a west spdx build document."""
    graph = json.loads(build_doc.read_text("utf-8"))["@graph"]
    document = next(e for e in graph if e["type"] == "SpdxDocument")
    prefixes = {m["prefix"]: m["namespace"] for m in document.get("namespaceMap", [])}
    elf = next(
        e for e in graph if e["type"] == "software_File" and e["name"] == "zephyr/zephyr.elf"
    )
    prefix, _, local = elf["spdxId"].partition(":")
    return prefixes[prefix] + local if prefix in prefixes else elf["spdxId"]


def write_image_sbom(path: Path, namespace: str, image: str, image_dir: Path) -> None:
    created = datetime.now(UTC).replace(microsecond=0).isoformat()
    created = created.replace("+00:00", "Z")
    elf_id = elf_spdx_id(path.parent / "build.jsonld")

    def element(local_id: str, body: dict) -> dict:
        return {**body, "spdxId": f"{namespace}{local_id}", "creationInfo": "_:creationinfo"}

    org = element("agent/rakwireless", {"type": "Organization", "name": "RAKwireless"})
    tool = element(
        "tool/generate-firmware-sbom", {"type": "Tool", "name": "RZI generate-firmware-sbom.py"}
    )
    files = []
    for name in IMAGE_FILES:
        artifact = image_dir / "zephyr" / name
        if not artifact.is_file():
            continue
        data = artifact.read_bytes()
        files.append(
            element(
                f"file/{name}",
                {
                    "type": "software_File",
                    "name": f"zephyr/{name}",
                    "software_fileKind": "file",
                    "software_primaryPurpose": "firmware",
                    "verifiedUsing": [
                        {"type": "Hash", "algorithm": algo, "hashValue": digest(data).hexdigest()}
                        for algo, digest in (("sha256", hashlib.sha256), ("sha512", hashlib.sha512))
                    ],
                },
            )
        )
    file_ids = [f["spdxId"] for f in files]
    generates = element(
        "relationship/generates",
        {"type": "Relationship", "from": elf_id, "relationshipType": "generates", "to": file_ids},
    )
    content = [org, tool, *files, generates]
    sbom = element(
        "sbom",
        {
            "type": "software_Sbom",
            "name": f"{image} firmware image",
            "software_sbomType": ["build"],
            "profileConformance": ["core", "software"],
            "rootElement": file_ids,
            "element": [e["spdxId"] for e in content],
        },
    )
    document = element(
        "document",
        {
            "type": "SpdxDocument",
            "name": f"{image} firmware image",
            "profileConformance": ["core", "software"],
            "rootElement": [sbom["spdxId"]],
            "element": [e["spdxId"] for e in (*content, sbom)],
            "import": [{"type": "ExternalMap", "externalSpdxId": elf_id}],
        },
    )
    creation = {
        "type": "CreationInfo",
        "@id": "_:creationinfo",
        "specVersion": "3.0.1",
        "created": created,
        "createdBy": [org["spdxId"]],
        "createdUsing": [tool["spdxId"]],
    }
    graph = [creation, *content, sbom, document]
    payload = {"@context": "https://spdx.org/rdf/3.0.1/spdx-context.jsonld", "@graph": graph}
    path.write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("build_dir", type=Path)
    parser.add_argument("-o", "--output-dir", type=Path)
    args = parser.parse_args()

    build_dir = args.build_dir.resolve()
    out_dir = (args.output_dir or build_dir / "spdx").resolve()
    found = images(build_dir)
    missing = [name for name, path in found.items() if not has_build_meta(path)]
    if missing:
        flags = " ".join(f"-D{name}_CONFIG_BUILD_OUTPUT_META=y" for name in missing)
        raise SystemExit(
            f"CONFIG_BUILD_OUTPUT_META is not enabled for: {', '.join(missing)}\n"
            f"Rebuild with: -- -DCONFIG_BUILD_OUTPUT_META=y {flags}"
        )

    run_id = uuid.uuid4()
    for name, image_dir in found.items():
        prefix = f"{NAMESPACE_ROOT}/{build_dir.name}/{name}/{run_id}"
        subprocess.run(
            [
                "west",
                "spdx",
                "-d",
                str(image_dir),
                "--spdx-version",
                "3.0",
                "-n",
                prefix,
                "-s",
                str(out_dir / name),
            ],
            check=True,
        )
        write_image_sbom(out_dir / name / "image.jsonld", f"{prefix}/image/", name, image_dir)
        print(f"{name}: {out_dir / name}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
