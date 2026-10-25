#!/usr/bin/env bash
# SPDX-FileCopyrightText: Copyright (c) 2026 RAKwireless
# SPDX-License-Identifier: Apache-2.0
#
# Zephyr gitlint for one proposed commit message.
#
#   scripts/check-commit-msg.sh <message-file>
#
# .githooks/commit-msg runs this before a commit is created. The config
# matches scripts/check-compliance.sh: Zephyr's .gitlint, with extra-path
# rewritten to this Zephyr tree and UC2 ignored. UC2 requires two
# whitespace-separated name tokens; the DCO check accepts a single-token
# committer name.

set -euo pipefail

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

if [[ $# -ne 1 || ! -f "$1" ]]; then
	echo "usage: scripts/check-commit-msg.sh <message-file>" >&2
	exit 1
fi

msg="$1"
if [[ "${msg}" != /* ]]; then
	msg="$(pwd)/${msg}"
fi

find_west_top() {
	local dir="${REPO}"
	while [[ "${dir}" != "/" ]]; do
		if [[ -f "${dir}/.west/config" ]]; then
			printf '%s\n' "${dir}"
			return 0
		fi
		dir="$(dirname "${dir}")"
	done
	return 1
}

find_zephyr() {
	if [[ -n "${ZEPHYR_BASE:-}" && -f "${ZEPHYR_BASE}/scripts/gitlint/zephyr_commit_rules.py" ]]; then
		printf '%s\n' "${ZEPHYR_BASE}"
		return 0
	fi
	local cfg
	cfg="$(git -C "${REPO}" config --get rzi.zephyrbase 2>/dev/null || true)"
	if [[ -n "${cfg}" && -f "${cfg}/scripts/gitlint/zephyr_commit_rules.py" ]]; then
		printf '%s\n' "${cfg}"
		return 0
	fi
	local top
	top="$(west -C "${REPO}" topdir 2>/dev/null || true)"
	if [[ -n "${top}" && -f "${top}/zephyr/scripts/gitlint/zephyr_commit_rules.py" ]]; then
		printf '%s\n' "${top}/zephyr"
		return 0
	fi
	if top="$(find_west_top)"; then
		local base
		base="$(git config -f "${top}/.west/config" --get zephyr.base 2>/dev/null || true)"
		base="${base:-zephyr}"
		if [[ -f "${top}/${base}/scripts/gitlint/zephyr_commit_rules.py" ]]; then
			printf '%s\n' "${top}/${base}"
			return 0
		fi
	fi
	return 1
}

find_gitlint() {
	if command -v gitlint >/dev/null 2>&1; then
		command -v gitlint
		return 0
	fi
	local top
	if top="$(find_west_top)" && [[ -x "${top}/.venv/bin/gitlint" ]]; then
		printf '%s\n' "${top}/.venv/bin/gitlint"
		return 0
	fi
	if [[ -x "${REPO}/.venv/bin/gitlint" ]]; then
		printf '%s\n' "${REPO}/.venv/bin/gitlint"
		return 0
	fi
	return 1
}

if ! ZEPHYR_BASE="$(find_zephyr)"; then
	echo "check-commit-msg: Zephyr checkout not found" >&2
	echo "  set ZEPHYR_BASE or: git config rzi.zephyrbase <path>" >&2
	exit 1
fi

if ! gitlint="$(find_gitlint)"; then
	echo "check-commit-msg: gitlint not found" >&2
	echo "  python3 -m pip install -r ${ZEPHYR_BASE}/scripts/requirements-compliance.txt" >&2
	exit 1
fi

cfg="$(mktemp)"
trap 'rm -f "${cfg}"' EXIT

# Keep this rewrite in sync with scripts/check-compliance.sh.
sed -e "s|^extra-path=.*|extra-path=${ZEPHYR_BASE}/scripts/gitlint|" \
	-e 's/^ignore=/ignore=UC2, /' \
	"${ZEPHYR_BASE}/.gitlint" > "${cfg}"

cd "${REPO}"
# --staged fills in the author so ignore-by-author-name can run. Without it
# gitlint warns and skips that rule on a message that is not a commit yet.
exec "${gitlint}" -C "${cfg}" --staged --msg-filename "${msg}"
