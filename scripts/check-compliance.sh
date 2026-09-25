#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
#
# Run Zephyr's scripts/ci/check_compliance.py against this module.
#
#   scripts/check-compliance.sh
#   scripts/check-compliance.sh origin/main..HEAD
#
# ZEPHYR_BASE, "git config rzi.zephyrbase", or a west workspace selects
# the Zephyr tree. Install that tree's scripts/requirements-actions.txt
# first; the compliance script imports those packages at startup.
#
# Checks that validate the Zephyr repository itself are excluded. ClangFormat
# is excluded the same way as zephyr/.github/workflows/compliance.yml;
# scripts/check-style.sh still enforces clang-format for the whole tree.

set -euo pipefail

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

# Same set Zephyr's compliance workflow excludes, plus checks that parse
# the Zephyr tree (Kconfig / sysbuild / "no module.yml in the zephyr repo")
# rather than the module under test.
EXCLUDES=(
	ClangFormat
	Kconfig
	KconfigBasic
	KconfigBasicNoModules
	KconfigHWMv2
	SysbuildKconfig
	SysbuildKconfigBasic
	SysbuildKconfigBasicNoModules
	ZephyrModuleFile
)

# Zephyr reports these as warnings. Every other non-empty report fails CI.
WARN_ONLY=(LicenseAndCopyrightCheck)

find_zephyr() {
	if [[ -n "${ZEPHYR_BASE:-}" && -f "${ZEPHYR_BASE}/scripts/ci/check_compliance.py" ]]; then
		printf '%s\n' "${ZEPHYR_BASE}"
		return 0
	fi
	local cfg
	cfg="$(git -C "${REPO}" config --get rzi.zephyrbase 2>/dev/null || true)"
	if [[ -n "${cfg}" && -f "${cfg}/scripts/ci/check_compliance.py" ]]; then
		printf '%s\n' "${cfg}"
		return 0
	fi
	local top
	top="$(west -C "${REPO}" topdir 2>/dev/null || true)"
	if [[ -n "${top}" && -f "${top}/zephyr/scripts/ci/check_compliance.py" ]]; then
		printf '%s\n' "${top}/zephyr"
		return 0
	fi
	return 1
}

if ! ZEPHYR_BASE="$(find_zephyr)"; then
	echo "check-compliance: Zephyr checkout not found" >&2
	echo "  set ZEPHYR_BASE or: git config rzi.zephyrbase <path>" >&2
	exit 1
fi
export ZEPHYR_BASE

if [[ $# -ge 1 ]]; then
	RANGE="$1"
elif [[ -n "${BASE_REF:-}" ]]; then
	RANGE="${BASE_REF}..HEAD"
else
	RANGE="HEAD~1..HEAD"
fi

created_conf=0
created_gitlint=0
cleanup() {
	if [[ "${created_conf}" == 1 ]]; then
		rm -f "${REPO}/.checkpatch.conf"
	fi
	if [[ "${created_gitlint}" == 1 ]]; then
		rm -f "${REPO}/.gitlint"
	fi
	rm -f "${REPO}/compliance.xml"
}
trap cleanup EXIT

# checkpatch.pl loads ./.checkpatch.conf and resolves --typedefsfile from
# the repository root. Point it at the Zephyr tree. EXECUTE_PERMISSIONS is
# the one local addition: hook and helper scripts must stay executable.
if [[ ! -e "${REPO}/.checkpatch.conf" ]]; then
	sed -e "s|^--typedefsfile=.*|--typedefsfile=${ZEPHYR_BASE}/scripts/checkpatch/typedefsfile|" \
		"${ZEPHYR_BASE}/.checkpatch.conf" > "${REPO}/.checkpatch.conf"
	printf '%s\n' '--ignore EXECUTE_PERMISSIONS' >> "${REPO}/.checkpatch.conf"
	created_conf=1
fi

# gitlint reads ./.gitlint. Zephyr's extra-path is relative to the Zephyr
# repository; rewrite it so the Zephyr commit rules load from this tree.
# UC2 requires two whitespace-separated name tokens. DCO checks the trailer
# against the committer, including a single-token name such as Daniel.Cao.
if [[ ! -e "${REPO}/.gitlint" ]]; then
	sed -e "s|^extra-path=.*|extra-path=${ZEPHYR_BASE}/scripts/gitlint|" \
		-e 's/^ignore=/ignore=UC2, /' \
		"${ZEPHYR_BASE}/.gitlint" > "${REPO}/.gitlint"
	created_gitlint=1
fi

cd "${REPO}"

args=()
for check in "${EXCLUDES[@]}"; do
	args+=(-e "${check}")
done

compliance=("${ZEPHYR_BASE}/scripts/ci/check_compliance.py")
mapfile -t all_checks < <(python3 "${compliance[@]}" -l)

# check_compliance.py exits non-zero for warn-only checks too. Zephyr's
# workflow ignores that status and fails only when some other report is
# non-empty. Match that here.
set +e
python3 "${compliance[@]}" --annotate -c "${RANGE}" "${args[@]}"
rc=$?
set -e

hard=0
reports=0
for name in "${all_checks[@]}"; do
	report="${name}.txt"
	if [[ ! -s "${report}" ]]; then
		rm -f "${report}"
		continue
	fi
	reports=1
	warn_only=0
	for allowed in "${WARN_ONLY[@]}"; do
		if [[ "${name}" == "${allowed}" ]]; then
			warn_only=1
		fi
	done
	if [[ "${warn_only}" == 1 ]]; then
		echo "::warning title=${name}::see the report below"
	else
		echo "::error title=${name}::see the report below"
		hard=1
	fi
	cat "${report}"
	rm -f "${report}"
done

if [[ "${hard}" == 1 ]]; then
	exit 1
fi
# A crash that produced no report is still a failure.
if [[ "${rc}" -ne 0 && "${reports}" == 0 ]]; then
	exit "${rc}"
fi
exit 0
