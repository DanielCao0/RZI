#!/usr/bin/env python3
# Copyright (c) 2026 RAKwireless
# SPDX-License-Identifier: Apache-2.0
"""Generate a source-level SBOM for the RZI Zephyr module.

This records RZI itself and the backend projects pinned in west.yml. It is
not a firmware image SBOM: Zephyr, MCUBoot, and SoC HALs come from the
consuming workspace. After a product build, run `west spdx` for that.

Outputs (default directory doc/sbom/):

  rzi.spdx.json   SPDX 2.3 JSON (NTIA minimum elements)
  rzi.cdx.json    CycloneDX 1.6 JSON

Run from the RZI repository root:

  python3 scripts/generate-sbom.py
"""
from __future__ import annotations

import argparse
import json
import re
import subprocess
import uuid
from datetime import datetime, timezone
from pathlib import Path
from typing import Any
from urllib.parse import quote

import yaml

REPO_ROOT = Path(__file__).resolve().parents[1]
DEFAULT_OUT = REPO_ROOT / "doc" / "sbom"
RZI_DOWNLOAD = "https://github.com/DanielCao0/RZI"
SPDX_LICENSE_LIST = "3.25"
CREATOR_TOOL = "RZI generate-sbom.py"

LICENSE_BY_PROJECT = {
    "rzi": {
        "spdx": "Apache-2.0",
        "copyright": "Copyright 2026 RAKwireless",
        "supplier": "Organization: RAKwireless",
    },
    "usp_zephyr": {
        "spdx": "BSD-3-Clause-Clear",
        "copyright": "Copyright Semtech Corporation 2025",
        "supplier": "Organization: Semtech Corporation",
    },
    "usp": {
        "spdx": "BSD-3-Clause-Clear",
        "copyright": "Copyright Semtech Corporation 2025",
        "supplier": "Organization: Semtech Corporation",
    },
}


def git(*args: str, cwd: Path = REPO_ROOT) -> str:
    result = subprocess.run(
        ["git", *args],
        cwd=cwd,
        check=True,
        capture_output=True,
        text=True,
    )
    return result.stdout.strip()


def read_version(path: Path) -> str:
    match = re.search(
        r'#define\s+RZI_VERSION_STRING\s+"([^"]+)"',
        path.read_text(encoding="utf-8"),
    )
    if not match:
        raise SystemExit(f"RZI_VERSION_STRING not found in {path}")
    return match.group(1)


def github_purl(url: str, revision: str) -> str:
    body = url.removeprefix("https://github.com/").removesuffix(".git")
    return f"pkg:github/{body}@{quote(revision, safe='')}"


def load_west_projects(path: Path) -> list[dict[str, Any]]:
    data = yaml.safe_load(path.read_text(encoding="utf-8"))
    return list(data["manifest"]["projects"])


def load_patch_targets(path: Path) -> dict[str, list[str]]:
    if not path.is_file():
        return {}
    data = yaml.safe_load(path.read_text(encoding="utf-8"))
    targets: dict[str, list[str]] = {}
    for entry in data.get("patches", []):
        module = entry.get("module")
        patch = entry.get("path")
        if module and patch:
            targets.setdefault(module, []).append(patch)
    return targets


def rzi_component(version: str, revision: str) -> dict[str, Any]:
    return {
        "name": "rzi",
        "version": version,
        "revision": revision,
        "download": f"{RZI_DOWNLOAD}.git",
        "homepage": RZI_DOWNLOAD,
        "purl": github_purl(RZI_DOWNLOAD, revision),
        "comment": (
            "Independent Zephyr module. Public C APIs live under include/rzi/. "
            "Backend protocol stacks are not part of the public ABI."
        ),
        **LICENSE_BY_PROJECT["rzi"],
    }


def west_component(project: dict[str, Any], patches: dict[str, list[str]]) -> dict[str, Any]:
    name = project["name"]
    url = project["url"]
    revision = str(project["revision"])
    license_info = LICENSE_BY_PROJECT.get(
        name,
        {
            "spdx": "NOASSERTION",
            "copyright": "NOASSERTION",
            "supplier": "NOASSERTION",
        },
    )
    comment = (
        f"Pinned by rzi/west.yml. Workspace path: {project.get('path', name)}."
    )
    applied = patches.get(name, [])
    if applied:
        comment += " RZI west patches applied: " + ", ".join(applied) + "."
    if project.get("submodules"):
        comment += " west clones this project with git submodules enabled."
    return {
        "name": name,
        "version": revision,
        "revision": revision,
        "download": url if url.endswith(".git") else f"{url}.git",
        "homepage": url.removesuffix(".git"),
        "purl": github_purl(url, revision),
        "comment": comment,
        **license_info,
    }


def spdx_package(comp: dict[str, Any], spdxid: str) -> dict[str, Any]:
    return {
        "SPDXID": spdxid,
        "name": comp["name"],
        "versionInfo": comp["version"],
        "downloadLocation": f"git+{comp['download']}@{comp['revision']}",
        "filesAnalyzed": False,
        "homepage": comp["homepage"],
        "licenseConcluded": comp["spdx"],
        "licenseDeclared": comp["spdx"],
        "copyrightText": comp["copyright"],
        "supplier": comp["supplier"],
        "originator": comp["supplier"],
        "primaryPackagePurpose": "LIBRARY",
        "comment": comp["comment"],
        "externalRefs": [
            {
                "referenceCategory": "PACKAGE-MANAGER",
                "referenceType": "purl",
                "referenceLocator": comp["purl"],
            }
        ],
    }


def cyclonedx_component(comp: dict[str, Any], bom_ref: str) -> dict[str, Any]:
    return {
        "type": "library",
        "bom-ref": bom_ref,
        "name": comp["name"],
        "version": comp["version"],
        "description": comp["comment"],
        "licenses": [{"license": {"id": comp["spdx"]}}],
        "copyright": comp["copyright"],
        "purl": comp["purl"],
        "supplier": {
            "name": comp["supplier"].removeprefix("Organization: "),
        },
        "externalReferences": [
            {"type": "website", "url": comp["homepage"]},
            {"type": "vcs", "url": f"{comp['download']}#{comp['revision']}"},
        ],
    }


def write_spdx(
    path: Path,
    namespace: str,
    created: str,
    root: dict[str, Any],
    deps: list[dict[str, Any]],
) -> None:
    packages = [spdx_package(root, "SPDXRef-Package-rzi")]
    relationships = [
        {
            "spdxElementId": "SPDXRef-DOCUMENT",
            "relatedSpdxElement": "SPDXRef-Package-rzi",
            "relationshipType": "DESCRIBES",
        }
    ]
    for dep in deps:
        spdxid = f"SPDXRef-Package-{dep['name']}"
        packages.append(spdx_package(dep, spdxid))
        relationships.append(
            {
                "spdxElementId": "SPDXRef-Package-rzi",
                "relatedSpdxElement": spdxid,
                "relationshipType": "DEPENDS_ON",
            }
        )
    document = {
        "spdxVersion": "SPDX-2.3",
        "dataLicense": "CC0-1.0",
        "SPDXID": "SPDXRef-DOCUMENT",
        "name": f"rzi-{root['version']}",
        "documentNamespace": namespace,
        "documentDescribes": ["SPDXRef-Package-rzi"],
        "creationInfo": {
            "created": created,
            "creators": [
                "Organization: RAKwireless",
                f"Tool: {CREATOR_TOOL}",
            ],
            "licenseListVersion": SPDX_LICENSE_LIST,
            "comment": (
                "Source-level SBOM of the RZI Zephyr module and the backend "
                "projects it pins in west.yml. Zephyr, MCUBoot, and SoC HALs "
                "are selected by the consuming application and are omitted. "
                "Use west spdx after a firmware build for an image SBOM."
            ),
        },
        "packages": packages,
        "relationships": relationships,
    }
    path.write_text(json.dumps(document, indent=2) + "\n", encoding="utf-8")


def write_cdx(
    path: Path,
    serial: str,
    created: str,
    root: dict[str, Any],
    deps: list[dict[str, Any]],
) -> None:
    root_ref = "pkg:rzi"
    components = []
    depends_on = []
    for dep in deps:
        ref = f"pkg:{dep['name']}"
        components.append(cyclonedx_component(dep, ref))
        depends_on.append(ref)
    document = {
        "bomFormat": "CycloneDX",
        "specVersion": "1.6",
        "serialNumber": serial,
        "version": 1,
        "metadata": {
            "timestamp": created,
            "tools": {
                "components": [
                    {
                        "type": "application",
                        "name": CREATOR_TOOL,
                        "publisher": "RAKwireless",
                    }
                ]
            },
            "authors": [{"name": "RAKwireless"}],
            "component": cyclonedx_component(root, root_ref),
            "properties": [
                {
                    "name": "sbom:scope",
                    "value": "rzi-module-source",
                },
                {
                    "name": "sbom:note",
                    "value": (
                        "Firmware image SBOMs must be generated with west spdx "
                        "from a product build; they include Zephyr and HALs."
                    ),
                },
            ],
        },
        "components": components,
        "dependencies": [
            {"ref": root_ref, "dependsOn": depends_on},
            *[{"ref": ref} for ref in depends_on],
        ],
    }
    path.write_text(json.dumps(document, indent=2) + "\n", encoding="utf-8")


def main() -> None:
    parser = argparse.ArgumentParser(description="Generate the RZI module SBOM")
    parser.add_argument(
        "-o",
        "--output-dir",
        type=Path,
        default=DEFAULT_OUT,
        help="Directory for rzi.spdx.json and rzi.cdx.json",
    )
    args = parser.parse_args()

    version = read_version(REPO_ROOT / "include" / "rzi" / "version.h")
    revision = git("rev-parse", "HEAD")
    created = datetime.now(timezone.utc).replace(microsecond=0).isoformat()
    created = created.replace("+00:00", "Z")
    namespace_uuid = uuid.uuid5(uuid.NAMESPACE_URL, f"{RZI_DOWNLOAD}/{revision}")
    namespace = f"{RZI_DOWNLOAD}/spdxdocs/rzi-{version}-{namespace_uuid}"
    serial = f"urn:uuid:{namespace_uuid}"

    root = rzi_component(version, revision)
    patches = load_patch_targets(REPO_ROOT / "zephyr" / "patches.yml")
    deps = [
        west_component(project, patches)
        for project in load_west_projects(REPO_ROOT / "west.yml")
    ]

    args.output_dir.mkdir(parents=True, exist_ok=True)
    spdx_path = args.output_dir / "rzi.spdx.json"
    cdx_path = args.output_dir / "rzi.cdx.json"
    write_spdx(spdx_path, namespace, created, root, deps)
    write_cdx(cdx_path, serial, created, root, deps)
    print(f"wrote {spdx_path.relative_to(REPO_ROOT)} (SPDX 2.3)")
    print(f"wrote {cdx_path.relative_to(REPO_ROOT)} (CycloneDX 1.6)")
    print(f"rzi {version} @ {revision}")
    for dep in deps:
        print(f"  depends on {dep['name']} @ {dep['revision']}")


if __name__ == "__main__":
    main()
