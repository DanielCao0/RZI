# RZI

基于 Zephyr RTOS 与 Semtech LoRa Basics Modem（USP）的 RAK4631 LoRaWAN 节点固件。

## 概述

- 硬件：RAK4631（nRF52840 + SX1262），Class A，OTAA 入网
- 协议栈：Semtech USP / LoRa Basics Modem（`usp_zephyr` + `usp`），非 Zephyr 自带 `CONFIG_LORAWAN`
- 应用行为：入网成功后每 60 s 在 port 1 上报 4 字节计数器；USB CDC 串口输出日志（115200 8N1）；蓝灯指示入网，绿灯指示发射
- 工作区：`app/` 即 west 清单仓（T2 拓扑），依赖版本集中在 `app/west.yml`
- 对 `usp_zephyr` 的本地修改走 Zephyr 官方 `west patch` 机制（`app/zephyr/patches.yml`）

## 仓库结构

| 路径 | 说明 |
|------|------|
| `app/` | 固件源码 + west 清单（`west.yml`） |
| `app/boards/rak4631_nrf52840.overlay` | 板级 overlay：SX1262 接 USP 驱动、LoRaWAN 密钥与区域 |
| `app/zephyr/` | `west patch` 补丁（`patches.yml` + 针对 `usp_zephyr` 的补丁集） |
| `doc/` | 设计文档（west 拓扑、补丁流程、USP/LBM 分层、板级 DTS 解析） |
| `zephyr-docker.sh` | 容器化构建入口（镜像、init、编译、补丁） |
| `flash-rak4631.sh` | J-Link SWD 烧录 |
| `Dockerfile.zephyr` | 构建镜像定义（Zephyr SDK + west） |

第三方源码树（`zephyr/`、`modules/`、`usp_zephyr/` 等）由 `west update` 拉取，不在本仓库内。

## 快速开始

前置条件：Docker；烧录需要 J-Link（SWD）。

```bash
git clone git@github.com:DanielCao0/RZI.git
cd RZI

./zephyr-docker.sh build-image   # 构建 Docker 镜像（仅首次）
./zephyr-docker.sh init          # west init -l app + west update（仅首次）
./zephyr-docker.sh build         # 编译 app/（rak4631/nrf52840）
./flash-rak4631.sh               # 烧录 app/build/zephyr/zephyr.hex
```

## 配置

入网参数在 `app/boards/rak4631_nrf52840.overlay` 的 `zephyr,user` 节点：

| 属性 | 说明 |
|------|------|
| `user-lorawan-device-eui` | DevEUI |
| `user-lorawan-join-eui` | JoinEUI（ChirpStack 默认全 0） |
| `user-lorawan-app-key` | OTAA AppKey（LoRaWAN 1.0；LBM `set_nwkkey`） |
| `user-lorawan-gen_app-key` | Gen App Key（LoRaWAN 1.1 AppKey；1.0 网络可与 AppKey 相同） |
| `user-lorawan-region` | `EU_868` / `US_915` / `CN_470` / `AS_923_GRP1` 等 |

修改后重新 `build` + 烧录生效。

## 构建脚本

```text
./zephyr-docker.sh build-image   构建 Dockerfile.zephyr 镜像
./zephyr-docker.sh init          初始化 west 工作区并打补丁
./zephyr-docker.sh build         编译 app/（默认 rak4631/nrf52840）
./zephyr-docker.sh sample        编译 usp_zephyr 官方 periodical_uplink
                                 （nRF52840 DK + SX126x 盾，非 RAK4631）
./zephyr-docker.sh patch         west patch clean + apply
./zephyr-docker.sh patch-list    查看补丁状态
./zephyr-docker.sh shell         进入容器 shell
./zephyr-docker.sh clangd        同步 compile_commands.json 供本机 clangd 跳转
```

## 文档

- [doc/README.md](./doc/README.md)：文档索引
- [Docker 环境逐步说明](./doc/zephyr-docker-environment-explained.md)
- [west 拓扑 T1 / T2 / T3](./doc/west-topology.md)
- [west patch 用法](./doc/west-patch.md)
- [USP / LBM / Zephyr 三者关系](./doc/usp-lbm-zephyr.md)

## 依赖版本

| 组件 | 来源 | 版本 |
|------|------|------|
| Zephyr | zephyrproject-rtos | 4.4.99（钉 commit `161f758`） |
| usp_zephyr | Lora-net | `main` |
| usp（LBM + RAC） | Lora-net | `main`（含 submodules） |
