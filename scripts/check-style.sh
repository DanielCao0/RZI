#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
#
# Zephyr coding-style checks for RZI.
#
#   scripts/check-style.sh           check the working tree (or HEAD when clean)
#   scripts/check-style.sh --staged  check staged changes (pre-commit hook mode)
#   scripts/check-style.sh --fix     apply clang-format in place
#
# clang-format uses the repository's own .clang-format (a copy of
# Zephyr's), so editors and plain clang-format invocations pick it up
# automatically. checkpatch.pl is taken from a Zephyr checkout, located
# via (in order) ZEPHYR_BASE, "git config rzi.zephyrbase", or a west
# workspace containing RZI:
#
#   ZEPHYR_BASE=~/rzi-workspace/zephyr scripts/check-style.sh

set -euo pipefail

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CLANG_FORMAT="${CLANG_FORMAT:-clang-format}"
if ! command -v "${CLANG_FORMAT}" >/dev/null 2>&1 &&
	[[ -x "${HOME}/.local/bin/clang-format" ]]; then
	CLANG_FORMAT="${HOME}/.local/bin/clang-format"
fi

# Mirror of zephyr/.checkpatch.conf (ignore list), plus MISSING_SIGN_OFF
# (the DCO sign-off is a zephyrproject contribution requirement, not
# enforced here) and EXECUTE_PERMISSIONS (hook and helper scripts must
# carry the execute bit; git requires it).
CHECKPATCH_IGNORE="SPLIT_STRING,SPDX_LICENSE_TAG,PRINTK_WITHOUT_KERN_LEVEL,\
VOLATILE,CONFIG_EXPERIMENTAL,PREFER_KERNEL_TYPES,PREFER_SECTION,AVOID_EXTERNS,\
NETWORKING_BLOCK_COMMENT_STYLE,DATE_TIME,MINMAX,CONST_STRUCT,FILE_PATH_CHANGES,\
C99_COMMENT_TOLERANCE,REPEATED_WORD,UNDOCUMENTED_DT_STRING,DT_SPLIT_BINDING_PATCH,\
DT_SCHEMA_BINDING_PATCH,TRAILING_SEMICOLON,COMPLEX_MACRO,\
MULTISTATEMENT_MACRO_USE_DO_WHILE,ENOSYS,IS_ENABLED_CONFIG,EXPORT_SYMBOL,\
COMPARISON_TO_NULL,MISSING_SIGN_OFF,EXECUTE_PERMISSIONS"

find_zephyr() {
	if [[ -n "${ZEPHYR_BASE:-}" && -f "${ZEPHYR_BASE}/scripts/checkpatch.pl" ]]; then
		printf '%s\n' "${ZEPHYR_BASE}"
		return 0
	fi
	local cfg
	cfg="$(git -C "${REPO}" config --get rzi.zephyrbase 2>/dev/null || true)"
	if [[ -n "${cfg}" && -f "${cfg}/scripts/checkpatch.pl" ]]; then
		printf '%s\n' "${cfg}"
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
	local fix="$1" staged="$2"
	local -a files

	if [[ "${staged}" == "yes" ]]; then
		mapfile -t files < <(git -C "${REPO}" diff --cached --name-only \
			--diff-filter=ACM -- '*.c' '*.h' | sed "s|^|${REPO}/|")
	else
		mapfile -t files < <(find "${REPO}" \( -name '*.c' -o -name '*.h' \) \
			-not -path '*/build/*' -not -path '*/.git/*')
	fi
	if [[ ${#files[@]} -eq 0 ]]; then
		echo "clang-format: no C files to check"
		return 0
	fi

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

run_doxygen_file_check() {
	local -a files
	local failed=no

	mapfile -t files < <(find "${REPO}/include/rzi" "${REPO}/src" -type f \
		\( -name '*.c' -o -name '*.h' \) | sort)
	for file in "${files[@]}"; do
		local header
		header="$(awk 'NR <= 20 { print }' "${file}")"
		if ! grep -Eq '^[[:space:]]*\*[[:space:]]+@file([[:space:]]|$)' <<<"${header}"; then
			echo "doxygen-file: missing @file: ${file#"${REPO}/"}"
			failed=yes
		fi
		if ! grep -Eq '^[[:space:]]*\*[[:space:]]+@brief[[:space:]]' <<<"${header}"; then
			echo "doxygen-file: missing @brief: ${file#"${REPO}/"}"
			failed=yes
		fi
	done
	if [[ "${failed}" == "yes" ]]; then
		return 1
	fi
	echo "doxygen-file: clean (${#files[@]} production files)"
}

run_checkpatch() {
	local staged="$1"
	local zb
	if ! zb="$(find_zephyr)"; then
		echo "checkpatch: skipped (Zephyr checkout not found;"
		echo "  set ZEPHYR_BASE or: git config rzi.zephyrbase <path>)"
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
	if [[ "${staged}" == "yes" ]]; then
		scope="staged changes"
		patch="$(git -C "${REPO}" diff --cached)"
	elif ! git -C "${REPO}" diff --quiet HEAD --; then
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
staged=no
case "${1:-}" in
	--fix) fix=yes ;;
	--staged) staged=yes ;;
	"") ;;
	*)
		echo "usage: $0 [--fix|--staged]" >&2
		exit 2
		;;
esac

rc=0
run_doxygen_file_check || rc=1
run_clang_format "${fix}" "${staged}" || rc=1
if [[ "${fix}" == "no" ]]; then
	run_checkpatch "${staged}" || rc=1
fi
exit "${rc}"
