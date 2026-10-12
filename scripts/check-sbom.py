#!/usr/bin/env python3
# SPDX-FileCopyrightText: Copyright (c) 2026 RAKwireless
# SPDX-License-Identifier: Apache-2.0
"""Check SBOMs against the NTIA / CISA 2025 minimum elements.

Usage:

  scripts/check-sbom.py [--strict] FILE [FILE ...]

FILE is an SPDX 3 JSON-LD document (*.jsonld) or a CycloneDX JSON document
(*.cdx.json). All SPDX 3 files given are merged into one graph, so the four
documents west spdx writes for one image are checked together.

Document level: author, tool, timestamp, generation context.
Component level, for every supply-chain component: name, version, producer
(supplier), software identifier (CPE, PURL or SWHID), hash, license, and a
dependency relationship. A SWHID, or a hash on every file the package
contains, counts as the component hash.

In SPDX 3 a supply-chain component is a software_Package with a download
location or an external identifier. Build-internal packages that west spdx
creates for CMake targets have neither and are not components. For west
spdx module packages, the source variant that contains compiled files is the
component and inherits identifiers and license from its reference package;
modules that contributed no files are skipped.

CycloneDX documents are also validated against the 1.6 JSON schema when
cyclonedx-python-lib is installed. Gaps are reported; --strict also makes
them fail the run.
"""

from __future__ import annotations

import argparse
import json
import sys
from collections import defaultdict
from pathlib import Path

IDENTIFIER_TYPES = {"cpe22", "cpe23", "packageUrl", "swhid", "gitoid", "swid"}
NO_LICENSE = {"expandedlicensing_NoAssertionLicense", "expandedlicensing_NoneLicense"}
DEPENDENCY_TYPES = {
    "dependsOn",
    "contains",
    "hasStaticLink",
    "hasDynamicLink",
    "hasProvidedDependency",
    "hasOptionalDependency",
    "describes",
}


def check_spdx3(paths: list[Path]) -> list[str]:
    graph = []
    for path in paths:
        graph.extend(json.loads(path.read_text("utf-8"))["@graph"])
    by_id = {e.get("spdxId") or e.get("@id"): e for e in graph}
    gaps = []

    infos = [e for e in graph if e["type"] == "CreationInfo"]
    for info in infos:
        if not info.get("createdBy"):
            gaps.append("SBOM author: CreationInfo without createdBy")
        if not info.get("createdUsing"):
            gaps.append("tool name: CreationInfo without createdUsing")
        if not info.get("created"):
            gaps.append("timestamp: CreationInfo without created")
    if not any(e["type"] == "software_Sbom" and e.get("software_sbomType") for e in graph):
        gaps.append("generation context: no software_Sbom with software_sbomType")

    licensed = set()
    related = set()
    roots = set()
    contains: dict[str, list[str]] = defaultdict(list)
    variant_of: dict[str, str] = {}
    for e in graph:
        if e["type"] in ("Relationship", "LifecycleScopedRelationship"):
            kind = e["relationshipType"]
            if kind in ("hasDeclaredLicense", "hasConcludedLicense"):
                if not NO_LICENSE.issuperset(e["to"]):
                    licensed.add(e["from"])
            elif kind == "hasVariant":
                for target in e["to"]:
                    variant_of[target] = e["from"]
            elif kind in DEPENDENCY_TYPES:
                related.add(e["from"])
                related.update(e["to"])
                if kind == "contains":
                    contains[e["from"]].extend(e["to"])
        elif e["type"] in ("SpdxDocument", "software_Sbom"):
            roots.update(e.get("rootElement", []))

    def identifiers_of(element: dict) -> set[str]:
        return {
            ref["externalIdentifierType"]
            for ref in element.get("externalIdentifier", [])
            if ref.get("externalIdentifierType") in IDENTIFIER_TYPES
        }

    for e in graph:
        if e["type"] != "software_Package":
            continue
        spdx_id = e["spdxId"]
        # west spdx describes each module twice: a reference package carrying
        # the CPE / PURL, and a source variant holding the compiled files. The
        # variant is the component; an empty one was not built in.
        if spdx_id in variant_of.values():
            continue
        parent = by_id.get(variant_of.get(spdx_id), {})
        if e.get("software_primaryPurpose") == "source" and not contains[spdx_id]:
            continue
        identifiers = identifiers_of(e) | identifiers_of(parent)
        download = e.get("software_downloadLocation", "NOASSERTION")
        if download == "NOASSERTION" and not identifiers:
            continue
        files = [by_id.get(f, {}) for f in contains[spdx_id]]
        files_hashed = bool(files) and all(f.get("verifiedUsing") for f in files)
        name = e.get("name", spdx_id)
        missing = []
        if not e.get("software_packageVersion"):
            missing.append("version")
        supplier = by_id.get(e.get("suppliedBy"), {})
        if not supplier.get("name"):
            missing.append("producer")
        if not identifiers:
            missing.append("identifier")
        if not (e.get("verifiedUsing") or "swhid" in identifiers or files_hashed):
            missing.append("hash")
        if spdx_id not in licensed and parent.get("spdxId") not in licensed:
            missing.append("license")
        if spdx_id not in related and spdx_id not in roots:
            missing.append("dependency")
        if missing:
            gaps.append(f"{name}: missing {', '.join(missing)}")
    return gaps


def cyclonedx_schema_errors(text: str) -> list[str]:
    try:
        from cyclonedx.schema import SchemaVersion
        from cyclonedx.validation.json import JsonStrictValidator
    except ImportError:
        print("  note: cyclonedx-python-lib not installed, schema validation skipped")
        return []
    error = JsonStrictValidator(SchemaVersion.V1_6).validate_str(text)
    return [f"schema: {error}"] if error else []


def check_cyclonedx(path: Path) -> list[str]:
    text = path.read_text("utf-8")
    bom = json.loads(text)
    meta = bom.get("metadata", {})
    gaps = cyclonedx_schema_errors(text)
    if not meta.get("authors") and not meta.get("manufacturer"):
        gaps.append("SBOM author: metadata.authors missing")
    if not meta.get("tools"):
        gaps.append("tool name: metadata.tools missing")
    if not meta.get("timestamp"):
        gaps.append("timestamp: metadata.timestamp missing")
    if not meta.get("lifecycles"):
        gaps.append("generation context: metadata.lifecycles missing")

    related = set()
    for dep in bom.get("dependencies", []):
        related.add(dep["ref"])
        related.update(dep.get("dependsOn", []))

    components = [meta["component"]] if "component" in meta else []
    components += bom.get("components", [])
    for comp in components:
        missing = []
        if not comp.get("version"):
            missing.append("version")
        if not (comp.get("supplier") or comp.get("manufacturer") or comp.get("authors")):
            missing.append("producer")
        if not (comp.get("purl") or comp.get("cpe") or comp.get("swhid") or comp.get("swid")):
            missing.append("identifier")
        if not (comp.get("hashes") or comp.get("swhid")):
            missing.append("hash")
        if not comp.get("licenses"):
            missing.append("license")
        if comp.get("bom-ref") not in related:
            missing.append("dependency")
        if missing:
            gaps.append(f"{comp.get('name', comp.get('bom-ref'))}: missing {', '.join(missing)}")
    return gaps


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("files", nargs="+", type=Path)
    parser.add_argument("--strict", action="store_true", help="fail when a gap is found")
    args = parser.parse_args()

    groups: dict[str, list[Path]] = defaultdict(list)
    for path in args.files:
        groups["spdx3" if path.suffix == ".jsonld" else "cyclonedx"].append(path)

    failed = False
    if groups["spdx3"]:
        failed |= report(
            "SPDX 3: " + ", ".join(map(str, groups["spdx3"])), check_spdx3(groups["spdx3"])
        )
    for path in groups["cyclonedx"]:
        failed |= report(f"CycloneDX: {path}", check_cyclonedx(path))
    return 1 if failed and args.strict else 0


def report(title: str, gaps: list[str]) -> bool:
    print(title)
    if not gaps:
        print("  all minimum elements present")
        return False
    for gap in gaps:
        print(f"  gap: {gap}")
    return True


if __name__ == "__main__":
    sys.exit(main())
