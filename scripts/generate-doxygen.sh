#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
#
# Generate the RZI Doxygen HTML tree from public headers and doc/*.md.
#
#   scripts/generate-doxygen.sh
#
# Output: doc/doxygen/html/index.html
# Warnings: doc/doxygen/warnings.log

set -euo pipefail

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DOXYFILE="${REPO}/doc/Doxyfile"
OUT="${REPO}/doc/doxygen"

if [[ -x "${HOME}/.local/bin/doxygen" ]]; then
	PATH="${HOME}/.local/bin:${PATH}"
fi
if ! command -v doxygen >/dev/null 2>&1; then
	echo "error: doxygen is required (sudo apt install doxygen, or put the official binary in ~/.local/bin)" >&2
	exit 2
fi

mkdir -p "${OUT}"
(
	cd "${REPO}/doc"
	doxygen "${DOXYFILE}"
)

echo "doxygen: HTML at ${OUT}/html/index.html"
if [[ -s "${OUT}/warnings.log" ]]; then
	echo "doxygen: warnings written to ${OUT}/warnings.log"
	wc -l "${OUT}/warnings.log"
fi
