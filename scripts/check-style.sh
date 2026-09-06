#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
#
# Zephyr coding-style checks for RZI.
#
#   scripts/check-style.sh         check formatting and patch style
#   scripts/check-style.sh --fix   apply clang-format in place
#
# clang-format uses the repository's own .clang-format (a copy of
# Zephyr's), so editors and plain clang-format invocations pick it up
# automatically. checkpatch.pl is taken from a Zephyr checkout, located
# via ZEPHYR_BASE or a west workspace containing RZI:
#
#   ZEPHYR_BASE=~/rzi-workspace/zephyr scripts/check-style.sh

set -euo pipefail

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CLANG_FORMAT="${CLANG_FORMAT:-clang-format}"

# Mirror of zephyr/.checkpatch.conf (ignore list), plus MISSING_SIGN_OFF:
# the DCO sign-off is a zephyrproject contribution requirement and is
# not enforced in this repository.
CHECKPATCH_IGNORE="SPLIT_STRING,SPDX_LICENSE_TAG,PRINTK_WITHOUT_KERN_LEVEL,\
VOLATILE,CONFIG_EXPERIMENTAL,PREFER_KERNEL_TYPES,PREFER_SECTION,AVOID_EXTERNS,\
NETWORKING_BLOCK_COMMENT_STYLE,DATE_TIME,MINMAX,CONST_STRUCT,FILE_PATH_CHANGES,\
C99_COMMENT_TOLERANCE,REPEATED_WORD,UNDOCUMENTED_DT_STRING,DT_SPLIT_BINDING_PATCH,\
DT_SCHEMA_BINDING_PATCH,TRAILING_SEMICOLON,COMPLEX_MACRO,\
MULTISTATEMENT_MACRO_USE_DO_WHILE,ENOSYS,IS_ENABLED_CONFIG,EXPORT_SYMBOL,\
COMPARISON_TO_NULL,MISSING_SIGN_OFF"

find_sources() {
	find "${REPO}" \( -name '*.c' -o -name '*.h' \) \
		-not -path '*/build/*' -not -path '*/.git/*'
}

find_zephyr() {
	if [[ -n "${ZEPHYR_BASE:-}" && -f "${ZEPHYR_BASE}/scripts/checkpatch.pl" ]]; then
		printf '%s\n' "${ZEPHYR_BASE}"
		return 0
	fi
	local top
	top="$(west -C "${REPO}" topdir 2>/dev/null || true)"
	if [[ -n "${top}" && -f "${top}/zephyr/scripts/checkpatch.pl" ]]; then
		printf '%s\n' "${top}/zephyr"
		return 0
	fi
	return 1
}

run_clang_format() {
	local fix="$1"
	local -a files
	mapfile -t files < <(find_sources)

	if [[ "${fix}" == "yes" ]]; then
		"${CLANG_FORMAT}" -i "${files[@]}"
		echo "clang-format: applied to ${#files[@]} files"
		return 0
	fi

	local out
	if out="$("${CLANG_FORMAT}" --dry-run --Werror "${files[@]}" 2>&1)"; then
		echo "clang-format: clean (${#files[@]} files)"
	else
		printf '%s\n' "${out}"
		echo "clang-format: issues found, run 'scripts/check-style.sh --fix'"
		return 1
	fi
}

run_checkpatch() {
	local zb
	if ! zb="$(find_zephyr)"; then
		echo "checkpatch: skipped (Zephyr checkout not found; set ZEPHYR_BASE)"
		return 0
	fi

	local -a args=(
		--no-tree
		--max-line-length=100
		--min-conf-desc-length=1
		--show-types
		--ignore "${CHECKPATCH_IGNORE}"
	)
	if [[ -f "${zb}/scripts/checkpatch/typedefsfile" ]]; then
		args+=(--typedefsfile "${zb}/scripts/checkpatch/typedefsfile")
	fi

	local patch scope
	if ! git -C "${REPO}" diff --quiet HEAD --; then
		scope="working tree vs HEAD"
		patch="$(git -C "${REPO}" diff HEAD)"
	else
		scope="HEAD commit"
		patch="$(git -C "${REPO}" show HEAD)"
	fi
	if [[ -z "${patch}" ]]; then
		echo "checkpatch: nothing to check"
		return 0
	fi

	echo "checkpatch: ${scope}"
	local out
	out="$(printf '%s\n' "${patch}" | perl "${zb}/scripts/checkpatch.pl" "${args[@]}" - || true)"
	printf '%s\n' "${out}" | tail -5
	if printf '%s\n' "${out}" | grep -q '^ERROR:'; then
		return 1
	fi
}

fix=no
if [[ "${1:-}" == "--fix" ]]; then
	fix=yes
fi

rc=0
run_clang_format "${fix}" || rc=1
if [[ "${fix}" == "no" ]]; then
	run_checkpatch || rc=1
fi
exit "${rc}"
