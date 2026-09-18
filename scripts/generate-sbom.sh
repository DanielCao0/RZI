#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
#
# Generate the RZI module source SBOM (SPDX 2.3 and CycloneDX 1.6).
#
#   scripts/generate-sbom.sh
#
# Output: doc/sbom/rzi.spdx.json
#         doc/sbom/rzi.cdx.json

set -euo pipefail

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
exec python3 "${REPO}/scripts/generate-sbom.py" "$@"
