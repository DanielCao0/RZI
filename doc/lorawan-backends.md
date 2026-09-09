# 编译两种 LoRaWAN backend

RZI 的应用只调用 `rzi_lorawan_*`。具体协议栈在**编译期**用 Kconfig
`choice` 选定，一份固件里只有一个 backend，运行时不能切换。

```text
CONFIG_RZI_LORAWAN_BACKEND_USP       默认：USP + LoRa Basics Modem
CONFIG_RZI_LORAWAN_BACKEND_ZEPHYR    Zephyr <zephyr/lorawan/lorawan.h>，下层 loramac-node
CONFIG_RZI_LORAWAN_BACKEND_TEST      仅单元测试
```

规划中的 `CONFIG_RZI_LORAWAN_BACKEND_ZEPHYR_LBM` 还没有实现，不要和上面的
Zephyr backend 混为一谈。

两套栈互斥：USP 要求 `CONFIG_LORA=n` 且 `CONFIG_LORAWAN=n`；Zephyr backend
会打开这两项，并用 loramac-node 驱动同一颗 SX1262。不要两套同时驱射频。

应用代码不用改。要换的是 **Kconfig、板级 overlay、独立的 build 目录**。
切换 backend 后请用 `-p always`，或换一个 `-d` 目录，避免沿用旧的
CMake 缓存。

合同差异和 FUOTA 缺口见 [Zephyr LoRaWAN API backend](./lorawan-backend-zephyr.md)。
ChirpStack 部署见 [FUOTA](./fuota.md)。

## 1. USP / LBM（默认，产品路径）

现有 `samples/lorawan/class_a`、`at`、`fuota` 都是这条路径。

必要配置：

```conf
CONFIG_RZI=y
CONFIG_RZI_LORAWAN=y
CONFIG_RZI_LORAWAN_BACKEND_USP=y
CONFIG_LORA=n
CONFIG_LORAWAN=n
```

`BACKEND_USP` 是 choice 默认值，不写也可以。`CONFIG_LORA=n` 必须保留，
否则 USP 选项会被 Kconfig 关掉。

硬件和 USP 的 SX1262 兼容层（`semtech,sx1262-new`）在产品板 `rzi_rak4631`
上，例程不再各放一份 overlay。启动见 [boot.md](./boot.md)。

在 west 工作区根目录：

```bash
west build -p always --sysbuild \
  -b rzi_rak4631/nrf52840 \
  -d build/rzi-fuota-usp \
  rzi/samples/lorawan/fuota
```

Class A、AT 把最后的路径换成 `rzi/samples/lorawan/class_a` 或
`rzi/samples/lorawan/at`。

容器里与产品 app 相同：

```bash
cd app
./scripts/container.sh build -p always --sysbuild \
  -b rzi_rak4631/nrf52840 \
  -d /workdir/build/rzi-fuota-usp \
  /workdir/rzi/samples/lorawan/fuota
```

烧录合并镜像：

```bash
cd app
./scripts/flash-rak4631.sh ../build/rzi-fuota-usp/merged.hex
```

确认选中的是 USP：

```bash
grep CONFIG_RZI_LORAWAN_BACKEND build/rzi-fuota-usp/zephyr/.config
```

应看到 `CONFIG_RZI_LORAWAN_BACKEND_USP=y`。

## 2. Zephyr `lorawan_*` + loramac-node

这条路径用 Zephyr 公共 LoRaWAN API，下层必须是 **loramac-node**，不要选
`LORA_MODULE_BACKEND_NATIVE` 或 `LORA_MODULE_BACKEND_LORA_BASICS_MODEM`。

必要配置：

```conf
CONFIG_RZI=y
CONFIG_RZI_LORAWAN=y
CONFIG_RZI_LORAWAN_BACKEND_ZEPHYR=y
CONFIG_LORA_MODULE_BACKEND_LORAMAC_NODE=y
CONFIG_LORAWAN_REGION_EU868=y
```

不要再写 `CONFIG_LORA=n` / `CONFIG_LORAWAN=n`。`BACKEND_ZEPHYR` 会打开
它们。`CONFIG_LORAWAN_REGION_*` 必须和 `rzi_lorawan_set_region()` 一致；
只编进 EU868 却在运行时设 US915，`start()` 会失败。

板级要求：

- 产品板 `rzi_rak4631` 默认是 USP 的 `semtech,sx1262-new`
- Zephyr backend 需要 overlay 把 `&lora` 改回官方 `semtech,sx1262`，并补回
  该 compatible 要求的属性
- USB 控制台和 `zephyr,user` 密钥仍放在应用 overlay

`zephyr.conf`：

```conf
CONFIG_RZI_LORAWAN_BACKEND_ZEPHYR=y
CONFIG_LORA_MODULE_BACKEND_LORAMAC_NODE=y
CONFIG_LORAWAN_REGION_EU868=y
```

```bash
west build -p always --sysbuild \
  -b rzi_rak4631/nrf52840 \
  -d build/rzi-fuota-zephyr \
  rzi/samples/lorawan/fuota \
  -- \
  -DEXTRA_CONF_FILE=zephyr.conf \
  -DDTC_OVERLAY_FILE=<your-zephyr-radio.overlay>
```

确认：

```bash
grep -E 'CONFIG_RZI_LORAWAN_BACKEND|CONFIG_LORA_MODULE_BACKEND' \
  build/rzi-fuota-zephyr/rzi_lorawan_fuota/zephyr/.config
```

应看到：

```text
CONFIG_RZI_LORAWAN_BACKEND_ZEPHYR=y
CONFIG_LORA_MODULE_BACKEND_LORAMAC_NODE=y
# CONFIG_LORA_MODULE_BACKEND_LORA_BASICS_MODEM is not set
# CONFIG_LORA_MODULE_BACKEND_NATIVE is not set
```

`lorawan_backend_zephyr.c` 里有 `BUILD_ASSERT`：不是 loramac-node 会编不过。

原生模拟器上的合同测试（不烧板）：

```bash
ZEPHYR_TOOLCHAIN_VARIANT=host west build -p always \
  -b native_sim \
  -d build/rzi-lorawan-backend-zephyr \
  rzi/tests/lorawan/backend_zephyr

ZEPHYR_TOOLCHAIN_VARIANT=host west build \
  -d build/rzi-lorawan-backend-zephyr \
  -t run
```

## 3. FUOTA 怎么选

`CONFIG_RZI_LORAWAN_FUOTA=y` 时，RZI 协调层（状态、回调、读镜像、可选
`rzi_fuota_apply()`）两边都会编进去。协议包仍在各自栈里。

| | USP / LBM | Zephyr + loramac-node |
| --- | --- | --- |
| Clock Sync / RMS / Fragmentation | LBM 包，事件完整 | Zephyr services，事件不完整 |
| 独立 GenAppKey 组播 | 支持 | join 吃不下独立 GenAppKey |
| ChirpStack 联调 | 现有 FUOTA sample | 尚未在 RAK4631 上按这条路径联调 |
| 建议 | 产品 FUOTA | 验证 RZI 合同 |

要打 ChirpStack FUOTA，用第 1 节的 USP 命令。Zephyr 路径上若
GenAppKey 必须等于 AppKey，并额外打开 `CONFIG_LORAWAN_SERVICES`、
`CONFIG_LORAWAN_APP_CLOCK_SYNC`、`CONFIG_LORAWAN_FRAG_TRANSPORT`；
组播再开 `CONFIG_LORAWAN_REMOTE_MULTICAST`（依赖 NVM settings）。
`CONFIG_RZI_LORAWAN_FUOTA` 会 imply 这些选项，依赖不满足时不会强开。

## 4. 常见错误

- 在 USP 的 `prj.conf` 里只加 `CONFIG_RZI_LORAWAN_BACKEND_ZEPHYR=y`，却留下
  `CONFIG_LORA=n`：choice 冲突或配置被丢掉。
- 在 `rzi_rak4631` 上开 Zephyr backend 却不改 `&lora` compatible：USP 驱动和
  loramac-node 会抢同一颗 SX1262。
- 两个 backend 共用一个 `-d` 且不加 `-p always`：CMake 缓存里仍是旧栈。
- 开了 `CONFIG_RZI_MCUBOOT` 却不带 `--sysbuild`：配置阶段失败。
- 把 `zephyr.hex` 当整机镜像烧：0x0 没有 MCUBoot。
- 编进的 `CONFIG_LORAWAN_REGION_*` 和 `rzi_lorawan_set_region()` 不一致。
