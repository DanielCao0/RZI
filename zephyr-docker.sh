#!/usr/bin/env bash
# 用 bash 解释执行本脚本
# 封装：构建镜像、首次 west init -l app、进容器、编译
set -euo pipefail
ROOT="$(cd "$(dirname "$0")" && pwd)"
IMAGE="${IMAGE:-rzi-zephyr:latest}"
WS="${ROOT}"

usage() {
  cat <<EOF
Usage: $0 <command> [args...]

  build-image   构建 Dockerfile.zephyr -> ${IMAGE}
  init          west init -l app + west update（已初始化则跳过）
  shell         进入容器
  build         west build（首次会自动 init）
                无参数且存在 app/CMakeLists.txt 时编 app/（rak4631）
  sample        编 usp_zephyr 官方 periodical_uplink
                （nRF52840 DK + SX126x 盾；不是 RAK4631）
  run           west build -t run（QEMU）
  clangd        把 Docker 的 compile_commands 改成本机路径（跳转定义）
  patch         west patch clean + apply（官方补丁，见 doc/west-patch.md）
  patch-list    west patch list
  烧录：./flash-rak4631.sh

Examples:
  $0 build-image
  $0 build
  $0 sample
  $0 build -p always -b rak4631/nrf52840 -d /workdir/app/build /workdir/app
  $0 shell
EOF
}

# west -p always 会按 CMakeCache 里的 ZEPHYR_BASE 去跑 cmake/pristine.cmake。
# 缓存若是本机 west 写下的 /home/... 路径，容器里不存在，pristine 直接失败。
# 在宿主机上删构建目录，容器里永远从空目录 configure（路径是 /workdir/...）。
wipe_host_build_dir() {
  local dir="$1"
  if [[ -d "$dir" ]]; then
    rm -rf "$dir"
  fi
}

run_docker() {
  local workdir="${DOCKER_WORKDIR:-/workdir}"
  local it=()
  if [[ -t 0 ]]; then it=(-it); fi
  mkdir -p "${ROOT}/app"
  docker run --rm "${it[@]}" --network host \
    --user "$(id -u):$(id -g)" \
    -e HOME=/tmp \
    -e ZEPHYR_BASE=/workdir/zephyr \
    -e GIT_CONFIG_COUNT=1 \
    -e GIT_CONFIG_KEY_0=http.version \
    -e GIT_CONFIG_VALUE_0=HTTP/1.1 \
    -v "${WS}:/workdir" \
    -w "${workdir}" \
    "${IMAGE}" \
    "$@"
}

# 本仓库 app/west.yml 当清单；复用已有 zephyr/，再拉 usp_zephyr + usp
ensure_workspace() {
  if [[ -d "${WS}/.west" ]]; then
    return 0
  fi
  if [[ ! -f "${WS}/app/west.yml" ]]; then
    echo "missing ${WS}/app/west.yml" >&2
    exit 1
  fi
  echo "First-time setup: west init -l app + west update (this takes a while)..."
  DOCKER_WORKDIR=/workdir run_docker bash -lc '
    set -e
    west init -l app
    n=0
    until west update; do
      n=$((n+1))
      if [ "$n" -ge 5 ]; then
        echo "west update still failing after 5 tries" >&2
        exit 1
      fi
      echo "west update retry $n ..."
      sleep 5
    done
    west zephyr-export || true
  '
  west_patch_apply
}

# 官方 west patch：清单在 app/zephyr/patches.yml
west_patch_apply() {
  if [[ ! -f "${ROOT}/app/zephyr/patches.yml" || ! -d "${ROOT}/.west" ]]; then
    return 0
  fi
  run_docker west patch clean
  run_docker west patch apply
}

west_patch_list() {
  run_docker west patch list
}

# Docker 里路径是 /workdir + /opt/zephyr-sdk；clangd 在本机要仓库路径 + arm-none-eabi-gcc
sync_compile_commands() {
  local src="${1:-${ROOT}/app/build/compile_commands.json}"
  local dst="${ROOT}/compile_commands.json"
  if [[ ! -f "${src}" ]]; then
    return 0
  fi
  python3 - "${ROOT}" "${src}" "${dst}" <<'PY'
import json, re, sys
root, src, dst = sys.argv[1], sys.argv[2], sys.argv[3]
host_cc = "arm-none-eabi-gcc"
docker_cc = "/opt/zephyr-sdk/gnu/arm-zephyr-eabi/bin/arm-zephyr-eabi-gcc"
with open(src, encoding="utf-8") as f:
    data = json.load(f)
for e in data:
    for k in ("directory", "file", "command", "output"):
        if k in e and isinstance(e[k], str):
            e[k] = e[k].replace("/workdir", root)
    if "command" in e:
        e["command"] = e["command"].replace(docker_cc, host_cc)
        e["command"] = re.sub(r" --sysroot=\S+", "", e["command"])
        e["command"] = re.sub(r" -specs=\S+", "", e["command"])
with open(dst, "w", encoding="utf-8") as f:
    json.dump(data, f, indent=1)
    f.write("\n")
print(f"clangd db: {dst} ({len(data)} files)")
PY
}

cmd="${1:-}"
case "${cmd}" in
  build-image)
    docker build -f "${ROOT}/Dockerfile.zephyr" \
      --build-arg UID="$(id -u)" \
      --build-arg GID="$(id -g)" \
      --build-arg USERNAME="$(id -u -n)" \
      -t "${IMAGE}" "${ROOT}"
    ;;
  init)
    if [[ -d "${WS}/.west" ]]; then
      echo "Already initialized: ${WS}"
      west_patch_apply
      exit 0
    fi
    ensure_workspace
    ;;
  shell)
    shift || true
    ensure_workspace
    west_patch_apply
    run_docker bash "$@"
    ;;
  build)
    shift || true
    ensure_workspace
    west_patch_apply
    if [[ $# -eq 0 ]]; then
      if [[ -f "${ROOT}/app/CMakeLists.txt" ]]; then
        wipe_host_build_dir "${ROOT}/app/build"
        set -- west build -p always -b rak4631/nrf52840 -d /workdir/app/build /workdir/app
      else
        set -- west build -p always -b qemu_x86 zephyr/samples/hello_world
      fi
    else
      set -- west build "$@"
    fi
    run_docker "$@"
    sync_compile_commands "${ROOT}/app/build/compile_commands.json"
    ;;
  sample)
    ensure_workspace
    west_patch_apply
    wipe_host_build_dir "${ROOT}/build-periodical-uplink"
    run_docker west build -p always \
      -b nrf52840dk/nrf52840 \
      --shield semtech_sx1261mb2bas \
      -d /workdir/build-periodical-uplink \
      usp_zephyr/samples/usp/lbm/periodical_uplink
    sync_compile_commands "${ROOT}/build-periodical-uplink/compile_commands.json"
    ;;
  clangd)
    sync_compile_commands "${ROOT}/app/build/compile_commands.json"
    ;;
  patch)
    west_patch_apply
    ;;
  patch-list)
    west_patch_list
    ;;
  run)
    shift || true
    ensure_workspace
    run_docker west build -t run "$@"
    ;;
  -h|--help|"")
    usage
    ;;
  *)
    usage
    exit 1
    ;;
esac
