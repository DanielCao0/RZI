# RZI 设备启动

RZI 不实现 bootloader。MCUBoot 源码仍是 west 拉下来的模块。RZI 负责的是
**客户怎么启用、哪块板怎么分区、怎么烧、FUOTA 怎么装到槽里**。

## 契约（所有产品板）

1. 选 RZI 产品板，不要用上游 `rak4631` / `rak3172` 当客户入口。
2. 打开 `CONFIG_RZI_MCUBOOT=y`。官方例程和产品 app 已经打开。
3. 用 **sysbuild** 编。没有 sysbuild 时配置会失败，避免只烧应用、0x0 没有 boot。
4. 首次和恢复：烧构建目录里的 **合并镜像**（`merged.hex`），不要烧 `zephyr.hex`。
5. 之后升级：signed MCUBoot 镜像写入 **slot1**（`image-1`），再走
   `rzi_fuota_apply()`。RZI 认分区名，不认某一块板的绝对地址。

分区表在产品板上，不在 RZI 核心 Kconfig 里。换板只补该板的 DTS 和烧录说明。

| 板名 | 全名 | west 目标 | 本轮状态 |
|---|---|---|---|
| `rzi_rak4631` | RZI RAK4631 | `rzi_rak4631/nrf52840` | 已接 MCUBoot 和 LoRaWAN 例程 |
| `rzi_rak3372` | RZI RAK3372 | `rzi_rak3372/stm32wle5xx` | 只占板名；LoRaWAN / 烧录未交付 |

nRF52840（RAK4631）分区同时给原生 Zephyr 和 ArduinoCore-zephyr 用：

| 分区 | 地址 | 大小 | 用途 |
|---|---|---|---|
| `mcuboot` | `0x00000` | 48 KB | 唯一 bootloader |
| `image-0` | `0x0c000` | 304 KB | RZI app 或 Arduino loader |
| `image-1` | `0x58000` | 304 KB | FUOTA / 换槽 |
| `user` | `0xa4000` | 336 KB | Arduino LLEXT sketch |
| `storage` | `0xf8000` | 32 KB | settings |

换模式只换 slot0 里签过名的镜像，不换 MCUboot。原生应用忽略 `user` 分区。

这和 RUI3 / 厂 Arduino boot **不兼容**：没有 `AT+BOOT`、`nrfutil`、UF2。
换到 RZI 是一次性 SWD 换皮，和 RAK 自己在 4631 与 4631-R 之间换 boot 同类。

开发构建使用 MCUBoot 默认密钥。量产请换成自己的签名密钥（本仓库尚未接 CI 签包）。

## 本轮实例：RZI RAK4631

```bash
west build -p always --sysbuild \
  -b rzi_rak4631/nrf52840 \
  -d build/rzi-class-a \
  rzi/samples/lorawan/class_a
```

产物：`build/rzi-class-a/merged.hex`（boot + 签名应用）。

产品 app（在 `app/` 里）：

```bash
./scripts/container.sh build
./scripts/flash-rak4631.sh
```

默认烧 `build/app/merged.hex`。

密钥写在应用 overlay（`app.overlay` 或 `boards/rzi_rak4631_nrf52840.overlay`），
不要改板级 DTS。射频、分区、USP SX1262 兼容已经在 `rzi_rak4631` 上。
