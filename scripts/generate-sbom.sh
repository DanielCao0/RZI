#!/usr/bin/env bash
# SPDX-FileCopyrightText: Copyright (c) 2026 RAKwireless
# SPDX-License-Identifier: Apache-2.0
#
# Generate the RZI module source SBOM (SPDX 3.0.1 and CycloneDX 1.6).
#
#   scripts/generate-sbom.sh [--allow-dirty] [-o DIR]
#
# Output: build/sbom/rzi-<version>-src.tar.gz
#         build/sbom/rzi.spdx.jsonld
#         build/sbom/rzi.cdx.json

set -euo pipefail

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
exec python3 "${REPO}/scripts/generate-sbom.py" "$@"
