#!/usr/bin/env python3
# SPDX-FileCopyrightText: Copyright (c) 2026 RAKwireless
# SPDX-License-Identifier: Apache-2.0
"""Generate the source-level SBOM for the RZI Zephyr module.

The SBOM describes RZI at HEAD and the projects it pins in west.yml. It is
not a firmware image SBOM: after a product build, run
scripts/generate-firmware-sbom.py for that.

Outputs (default directory build/sbom/):

  rzi-<version>-src.tar.gz  git archive of HEAD, the hashed source artifact
  rzi.spdx.jsonld           SPDX 3.0.1 JSON-LD (core, software, simpleLicensing)
  rzi.cdx.json              CycloneDX 1.6 JSON

Run from the RZI repository root:

  python3 scripts/generate-sbom.py

SOURCE_DATE_EPOCH, when set, fixes the creation timestamp.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import subprocess
import sys
import uuid
from dataclasses import dataclass, field
from datetime import UTC, datetime
from pathlib import Path
from typing import Any

import yaml
from reuse.project import Project

REPO_ROOT = Path(__file__).resolve().parents[1]
DEFAULT_OUT = REPO_ROOT / "build" / "sbom"
RZI_URL = "https://github.com/DanielCao0/RZI"
TOOL_NAME = "RZI generate-sbom.py"
SPDX_LICENSE_LIST = "3.25.0"
SPDX_CONTEXT = "https://spdx.org/rdf/3.0.1/spdx-context.jsonld"

RAKWIRELESS = "RAKwireless"
SEMTECH = "Semtech Corporation"
ZEPHYR_PROJECT = "The Zephyr Project"

# Every project in west.yml needs an entry: an unknown dependency fails the
# run instead of entering the SBOM without a supplier or license.
WEST_PROJECT_META = {
    "zephyr": {
        "supplier": ZEPHYR_PROJECT,
        "license": "Apache-2.0",
        "copyright": "NOASSERTION",
        "cpe": "cpe:2.3:o:zephyrproject:zephyr:{version}:{update}:*:*:*:*:*:*",
        "comment": (
            "Zephyr revision RZI is validated against. An application manifest "
            "that pins its own Zephyr revision replaces this one."
        ),
    },
    "usp_zephyr": {
        "supplier": SEMTECH,
        "license": "BSD-3-Clause-Clear",
        "copyright": "Copyright Semtech Corporation 2025",
        "cpe": None,
        "comment": "Zephyr integration of the Semtech USP LoRaWAN stack.",
    },
    "usp": {
        "supplier": SEMTECH,
        "license": "BSD-3-Clause-Clear",
        "copyright": "Copyright Semtech Corporation 2025",
        "cpe": None,
        "comment": "Semtech USP LoRaWAN stack and LoRa Basics Modem.",
    },
    "lora-basics-modem": {
        "supplier": ZEPHYR_PROJECT,
        "license": "BSD-3-Clause-Clear",
        "copyright": "Copyright Semtech Corporation 2023",
        "cpe": None,
        "comment": (
            "Zephyr Project fork of Semtech LoRa Basics Modem."
        ),
    },
}

PURPOSE_BY_SUFFIX = {
    ".c": "source",
    ".h": "source",
    ".py": "source",
    ".sh": "source",
    ".cmake": "source",
    ".patch": "patch",
    ".md": "documentation",
    ".rst": "documentation",
    ".txt": "documentation",
    ".dts": "configuration",
    ".dtsi": "configuration",
    ".overlay": "configuration",
    ".conf": "configuration",
    ".yml": "configuration",
    ".yaml": "configuration",
    ".toml": "configuration",
    ".json": "configuration",
    ".bin": "data",
}


@dataclass
class Component:
    name: str
    version: str
    revision: str
    url: str
    supplier: str
    license: str
    copyright: str
    comment: str
    purl: str
    cpe: str | None = None
    patches: list[str] = field(default_factory=list)

    @property
    def swhid(self) -> str:
        return f"swh:1:rev:{self.revision}"

    @property
    def download(self) -> str:
        return f"git+{self.url}.git@{self.revision}"


@dataclass
class SourceFile:
    path: str
    sha256: str
    licenses: list[str]
    copyrights: list[str]


def git(*args: str) -> str:
    result = subprocess.run(
        ["git", *args], cwd=REPO_ROOT, check=True, capture_output=True, text=True
    )
    return result.stdout.strip()


def read_version(path: Path) -> str:
    match = re.search(r'#define\s+RZI_VERSION_STRING\s+"([^"]+)"', path.read_text("utf-8"))
    if not match:
        raise SystemExit(f"RZI_VERSION_STRING not found in {path}")
    return match.group(1)


def module_references(path: Path) -> tuple[str, str]:
    """Return the (CPE, PURL) declared in zephyr/module.yml."""
    data = yaml.safe_load(path.read_text("utf-8"))
    refs = data.get("security", {}).get("external-references", [])
    cpes = [ref for ref in refs if ref.startswith("cpe:2.3:")]
    purls = [ref for ref in refs if ref.startswith("pkg:")]
    if len(cpes) != 1 or len(purls) != 1:
        raise SystemExit(f"{path}: security.external-references needs one CPE and one PURL")
    return cpes[0], purls[0]


def github_slug(url: str) -> str:
    return url.removeprefix("https://github.com/").removesuffix(".git")


def revision_tags(manifest_text: str) -> dict[str, str]:
    """Map each pinned SHA to the release tag noted in its trailing comment."""
    tags = {}
    for match in re.finditer(r"revision:\s*([0-9a-f]{40})\s*#\s*(\S+)", manifest_text):
        tags[match.group(1)] = match.group(2)
    return tags


def zephyr_cpe(template: str, tag: str) -> str:
    version, _, update = tag.removeprefix("v").partition("-")
    return template.format(version=version, update=update or "*")


def patch_targets(path: Path) -> dict[str, list[str]]:
    if not path.is_file():
        return {}
    targets: dict[str, list[str]] = {}
    for entry in yaml.safe_load(path.read_text("utf-8")).get("patches", []):
        targets.setdefault(entry["module"], []).append(f"zephyr/patches/{entry['path']}")
    return targets


def west_components(manifest: Path, patches: dict[str, list[str]]) -> list[Component]:
    text = manifest.read_text("utf-8")
    tags = revision_tags(text)
    components = []
    for project in yaml.safe_load(text)["manifest"]["projects"]:
        name = project["name"]
        meta = WEST_PROJECT_META.get(name)
        if meta is None:
            raise SystemExit(f"west.yml project '{name}' has no entry in WEST_PROJECT_META")
        revision = str(project["revision"])
        tag = tags.get(revision)
        url = project["url"].removesuffix(".git")
        comment = meta["comment"] + f" Pinned by rzi/west.yml at {revision}."
        applied = patches.get(name, [])
        if applied:
            comment += " RZI applies west patches: " + ", ".join(applied) + "."
        components.append(
            Component(
                name=name,
                version=tag or revision,
                revision=revision,
                url=url,
                supplier=meta["supplier"],
                license=meta["license"],
                copyright=meta["copyright"],
                comment=comment,
                purl=f"pkg:github/{github_slug(url)}@{tag or revision}",
                cpe=zephyr_cpe(meta["cpe"], tag) if meta["cpe"] and tag else None,
                patches=applied,
            )
        )
    return components


def tracked_files(tree: str) -> list[SourceFile]:
    """Hash every file in a git tree and read its REUSE information."""
    listing = git("ls-tree", "-r", "-z", "--format=%(objectname) %(path)", tree)
    entries = [line.split(" ", 1) for line in listing.split("\0") if line]
    blobs = subprocess.run(
        ["git", "cat-file", "--batch"],
        cwd=REPO_ROOT,
        input="".join(f"{oid}\n" for oid, _ in entries).encode(),
        check=True,
        capture_output=True,
    ).stdout
    project = Project.from_directory(REPO_ROOT)
    files = []
    offset = 0
    for _, path in entries:
        header_end = blobs.index(b"\n", offset)
        size = int(blobs[offset:header_end].split()[2])
        content = blobs[header_end + 1 : header_end + 1 + size]
        offset = header_end + 1 + size + 1
        licenses: set[str] = set()
        copyrights: set[str] = set()
        for info in project.reuse_info_of(REPO_ROOT / path):
            licenses.update(str(expr) for expr in info.spdx_expressions)
            copyrights.update(str(notice) for notice in info.copyright_notices)
        files.append(
            SourceFile(
                path=path,
                sha256=hashlib.sha256(content).hexdigest(),
                licenses=sorted(licenses),
                copyrights=sorted(copyrights),
            )
        )
    return files


def write_archive(path: Path, prefix: str, tree: str) -> dict[str, str]:
    git("archive", "--format=tar.gz", f"--prefix={prefix}/", "-o", str(path), tree)
    data = path.read_bytes()
    return {
        "sha256": hashlib.sha256(data).hexdigest(),
        "sha512": hashlib.sha512(data).hexdigest(),
    }


def spdx_hash(algorithm: str, value: str) -> dict[str, str]:
    return {"type": "Hash", "algorithm": algorithm, "hashValue": value}


def spdx_identifier(kind: str, value: str) -> dict[str, str]:
    return {"type": "ExternalIdentifier", "externalIdentifierType": kind, "identifier": value}


def file_purpose(path: str) -> str:
    if path.startswith("tests/"):
        return "test"
    name = Path(path).name
    if name == "Doxyfile":
        return "documentation"
    if name.startswith("Kconfig") or name.endswith("_defconfig") or name == "sysbuild.conf":
        return "configuration"
    if name == "CMakeLists.txt":
        return "source"
    return PURPOSE_BY_SUFFIX.get(Path(path).suffix, "file")


class Spdx3Graph:
    def __init__(self, namespace: str, created: str):
        self.ns = namespace
        self.graph: list[dict[str, Any]] = []
        self.elements: list[str] = []
        self.licenses: dict[str, str] = {}
        self.agents: dict[str, str] = {}
        self.relationship_count = 0
        self.graph.append(
            {
                "type": "CreationInfo",
                "@id": "_:creationinfo",
                "specVersion": "3.0.1",
                "created": created,
                "createdBy": [self.agent(RAKWIRELESS)],
                "createdUsing": [
                    self.add({"type": "Tool", "name": TOOL_NAME}, "tool/generate-sbom")
                ],
            }
        )

    def add(self, element: dict[str, Any], local_id: str) -> str:
        spdx_id = f"{self.ns}{local_id}"
        self.graph.append({**element, "spdxId": spdx_id, "creationInfo": "_:creationinfo"})
        self.elements.append(spdx_id)
        return spdx_id

    def agent(self, name: str) -> str:
        if name not in self.agents:
            slug = re.sub(r"[^a-z0-9]+", "-", name.lower()).strip("-")
            self.agents[name] = self.add({"type": "Organization", "name": name}, f"agent/{slug}")
        return self.agents[name]

    def license(self, expression: str) -> str:
        if expression not in self.licenses:
            slug = re.sub(r"[^A-Za-z0-9.-]+", "-", expression)
            self.licenses[expression] = self.add(
                {
                    "type": "simplelicensing_LicenseExpression",
                    "simplelicensing_licenseExpression": expression,
                    "simplelicensing_licenseListVersion": SPDX_LICENSE_LIST,
                },
                f"license/{slug}",
            )
        return self.licenses[expression]

    def relate(self, source: str, kind: str, targets: list[str]) -> None:
        if not targets:
            return
        self.relationship_count += 1
        self.add(
            {"type": "Relationship", "from": source, "relationshipType": kind, "to": targets},
            f"relationship/{self.relationship_count}",
        )

    def licensed(self, element: str, expressions: list[str]) -> None:
        if not expressions:
            return
        if len(expressions) == 1:
            expression = expressions[0]
        else:
            expression = " AND ".join(f"({e})" if " " in e else e for e in expressions)
        license_id = self.license(expression)
        self.relate(element, "hasDeclaredLicense", [license_id])
        self.relate(element, "hasConcludedLicense", [license_id])


def spdx_package(graph: Spdx3Graph, comp: Component, hashes: dict[str, str]) -> str:
    identifiers = [
        spdx_identifier("packageUrl", comp.purl),
        spdx_identifier("swhid", comp.swhid),
    ]
    if comp.cpe:
        identifiers.insert(0, spdx_identifier("cpe23", comp.cpe))
    element: dict[str, Any] = {
        "type": "software_Package",
        "name": comp.name,
        "software_packageVersion": comp.version,
        "software_downloadLocation": comp.download,
        "software_homePage": comp.url,
        "software_copyrightText": comp.copyright,
        "software_primaryPurpose": "library",
        "suppliedBy": graph.agent(comp.supplier),
        "originatedBy": [graph.agent(comp.supplier)],
        "externalIdentifier": identifiers,
        "comment": comp.comment,
    }
    if hashes:
        element["verifiedUsing"] = [spdx_hash(algo, value) for algo, value in hashes.items()]
    package_id = graph.add(element, f"package/{comp.name}")
    graph.licensed(package_id, [comp.license])
    return package_id


def write_spdx3(
    path: Path,
    namespace: str,
    created: str,
    root: Component,
    archive_hashes: dict[str, str],
    deps: list[Component],
    files: list[SourceFile],
) -> None:
    graph = Spdx3Graph(namespace, created)
    root_id = spdx_package(graph, root, archive_hashes)

    file_ids: dict[str, str] = {}
    for index, source in enumerate(files):
        file_id = graph.add(
            {
                "type": "software_File",
                "name": source.path,
                "software_fileKind": "file",
                "software_primaryPurpose": file_purpose(source.path),
                "software_copyrightText": "\n".join(source.copyrights) or "NOASSERTION",
                "verifiedUsing": [spdx_hash("sha256", source.sha256)],
            },
            f"file/{index}",
        )
        file_ids[source.path] = file_id
        graph.licensed(file_id, source.licenses)
    graph.relate(root_id, "contains", list(file_ids.values()))

    dep_ids = []
    for dep in deps:
        dep_id = spdx_package(graph, dep, {})
        dep_ids.append(dep_id)
        graph.relate(dep_id, "patchedBy", [file_ids[p] for p in dep.patches if p in file_ids])
    graph.relate(root_id, "dependsOn", dep_ids)

    content = list(graph.elements)
    sbom_id = graph.add(
        {
            "type": "software_Sbom",
            "name": f"rzi-{root.version} source SBOM",
            "software_sbomType": ["source"],
            "profileConformance": ["core", "software", "simpleLicensing"],
            "rootElement": [root_id],
            "element": content,
        },
        "sbom",
    )
    data_license = graph.license("CC0-1.0")
    graph.add(
        {
            "type": "SpdxDocument",
            "name": f"rzi-{root.version}",
            "dataLicense": data_license,
            "profileConformance": ["core", "software", "simpleLicensing"],
            "rootElement": [sbom_id],
            "element": list(graph.elements),
            "comment": (
                "Source-level SBOM of the RZI Zephyr module at a single commit and "
                "the projects it pins in west.yml. Firmware images need their own "
                "SBOM from west spdx; see doc/sbom.md."
            ),
        },
        "document",
    )
    document = {"@context": SPDX_CONTEXT, "@graph": graph.graph}
    path.write_text(json.dumps(document, indent=2) + "\n", encoding="utf-8")


def cyclonedx_component(
    comp: Component, bom_ref: str, hashes: dict[str, str], rzi_revision: str
) -> dict[str, Any]:
    component: dict[str, Any] = {
        "type": "library",
        "bom-ref": bom_ref,
        "supplier": {"name": comp.supplier},
        "name": comp.name,
        "version": comp.version,
        "description": comp.comment,
        "licenses": [{"license": {"id": comp.license}}],
        "purl": comp.purl,
        "swhid": [comp.swhid],
        "externalReferences": [
            {"type": "website", "url": comp.url},
            {"type": "vcs", "url": f"{comp.url}.git#{comp.revision}"},
        ],
    }
    if comp.copyright != "NOASSERTION":
        component["copyright"] = comp.copyright
    if comp.cpe:
        component["cpe"] = comp.cpe
    if hashes:
        component["hashes"] = [
            {"alg": algo.upper().replace("SHA", "SHA-"), "content": value}
            for algo, value in hashes.items()
        ]
    if comp.patches:
        component["pedigree"] = {
            "patches": [
                {"type": "unofficial", "diff": {"url": f"{RZI_URL}/blob/{rzi_revision}/{p}"}}
                for p in comp.patches
            ]
        }
    return component


def write_cdx(
    path: Path,
    serial: str,
    created: str,
    root: Component,
    archive_name: str,
    archive_hashes: dict[str, str],
    deps: list[Component],
) -> None:
    root_component = cyclonedx_component(root, "rzi", archive_hashes, root.revision)
    root_component["externalReferences"].append(
        {
            "type": "distribution",
            "url": f"{RZI_URL}/releases/download/v{root.version}/{archive_name}",
        }
    )
    components = [cyclonedx_component(dep, dep.name, {}, root.revision) for dep in deps]
    document = {
        "$schema": "http://cyclonedx.org/schema/bom-1.6.schema.json",
        "bomFormat": "CycloneDX",
        "specVersion": "1.6",
        "serialNumber": serial,
        "version": 1,
        "metadata": {
            "timestamp": created,
            "lifecycles": [{"phase": "pre-build"}],
            "tools": {
                "components": [{"type": "application", "name": TOOL_NAME, "publisher": RAKWIRELESS}]
            },
            "authors": [{"name": RAKWIRELESS}],
            "manufacturer": {"name": RAKWIRELESS},
            "supplier": {"name": RAKWIRELESS},
            "component": root_component,
        },
        "components": components,
        "dependencies": [
            {"ref": "rzi", "dependsOn": [dep.name for dep in deps]},
            *[{"ref": dep.name} for dep in deps],
        ],
    }
    path.write_text(json.dumps(document, indent=2) + "\n", encoding="utf-8")


def creation_time() -> str:
    epoch = os.environ.get("SOURCE_DATE_EPOCH")
    moment = datetime.fromtimestamp(int(epoch), UTC) if epoch else datetime.now(UTC)
    return moment.replace(microsecond=0).isoformat().replace("+00:00", "Z")


def main() -> None:
    parser = argparse.ArgumentParser(description="Generate the RZI module source SBOM")
    parser.add_argument(
        "-o", "--output-dir", type=Path, default=DEFAULT_OUT, help="output directory"
    )
    parser.add_argument(
        "--allow-dirty",
        action="store_true",
        help="describe staged and modified tracked files instead of HEAD (development only)",
    )
    args = parser.parse_args()

    tree = "HEAD"
    if git("status", "--porcelain", "--untracked-files=no"):
        if not args.allow_dirty:
            raise SystemExit(
                "tracked files have uncommitted changes; the SBOM describes HEAD. "
                "Commit first or pass --allow-dirty."
            )
        tree = git("stash", "create")

    version = read_version(REPO_ROOT / "include" / "rzi" / "version.h")
    cpe, purl = module_references(REPO_ROOT / "zephyr" / "module.yml")
    if f":{version}:" not in cpe or not purl.endswith(f"@v{version}"):
        raise SystemExit(f"zephyr/module.yml references do not match RZI version {version}")

    revision = git("rev-parse", "HEAD")
    created = creation_time()
    doc_uuid = uuid.uuid5(uuid.NAMESPACE_URL, f"{RZI_URL}/{revision}")
    namespace = f"{RZI_URL}/spdx/rzi-{version}-{doc_uuid}/"

    root = Component(
        name="rzi",
        version=version if tree == "HEAD" else f"{version}+dirty",
        revision=revision,
        url=RZI_URL,
        supplier=RAKWIRELESS,
        license="Apache-2.0",
        copyright="Copyright (c) 2026 RAKwireless",
        comment=(
            "Independent Zephyr module. Public C APIs live under include/rzi/. "
            "Backend protocol stacks are not part of the public ABI."
        ),
        purl=purl,
        cpe=cpe,
    )
    deps = west_components(
        REPO_ROOT / "west.yml", patch_targets(REPO_ROOT / "zephyr" / "patches.yml")
    )

    args.output_dir.mkdir(parents=True, exist_ok=True)
    archive_name = f"rzi-{version}-src.tar.gz"
    archive_hashes = write_archive(args.output_dir / archive_name, f"rzi-{version}", tree)
    files = tracked_files(tree)
    # License texts and REUSE.toml are exempt under the REUSE specification.
    exempt = re.compile(r"^(LICENSES/|REUSE\.toml$|(LICENSE|COPYING)(\.\w+)?$)")
    unlicensed = [
        f.path for f in files if (not f.licenses or not f.copyrights) and not exempt.match(f.path)
    ]
    if unlicensed:
        raise SystemExit("files without REUSE license or copyright:\n  " + "\n  ".join(unlicensed))

    write_spdx3(
        args.output_dir / "rzi.spdx.jsonld",
        namespace,
        created,
        root,
        archive_hashes,
        deps,
        files,
    )
    write_cdx(
        args.output_dir / "rzi.cdx.json",
        f"urn:uuid:{doc_uuid}",
        created,
        root,
        archive_name,
        archive_hashes,
        deps,
    )

    print(f"rzi {version} @ {revision}: {len(files)} files")
    for dep in deps:
        print(f"  depends on {dep.name} {dep.version}")
    for name in (archive_name, "rzi.spdx.jsonld", "rzi.cdx.json"):
        print(f"wrote {args.output_dir / name}")


if __name__ == "__main__":
    sys.exit(main())
