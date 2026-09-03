#!/usr/bin/env bash
# J-Link SWD 烧 app/build/zephyr/zephyr.hex 到 RAK4631（nRF52840）。
# 在宿主机跑，不要进 Docker。SWD 接底板，不是 USB 线。
set -euo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"
HEX="${ROOT}/app/build/zephyr/zephyr.hex"

if [[ "${1:-}" == "-h" || "${1:-}" == "--help" ]]; then
  echo "Usage: $0 [--build]"
  echo "  --build  先 ./zephyr-docker.sh build 再烧"
  exit 0
fi

if [[ "${1:-}" == "--build" ]]; then
  "${ROOT}/zephyr-docker.sh" build
fi

[[ -f "${HEX}" ]] || {
  echo "error: missing ${HEX}（先 ./zephyr-docker.sh build）" >&2
  exit 1
}

command -v JLinkExe >/dev/null || {
  echo "error: 找不到 JLinkExe" >&2
  exit 1
}

script="$(mktemp --suffix=.jlink)"
trap 'rm -f "${script}"' EXIT
cat > "${script}" <<EOF
si 1
speed 4000
device nRF52840_xxAA
connect
halt
loadfile ${HEX}
r
g
exit
EOF

JLinkExe -nogui 1 -ExitOnError 1 -CommanderScript "${script}"
echo "done: ${HEX}"
