# `rak4631_nrf52840.dts` 逐行说明

源文件：[zephyr/boards/rakwireless/rak4631/rak4631_nrf52840.dts](../zephyr/boards/rakwireless/rak4631/rak4631_nrf52840.dts)

这是 Zephyr 树里的**板级设备树**（官方，不要改）。描述 RAK4631：nRF52840 + 板上焊死的 SX1262。你的 [app/boards/rak4631_nrf52840.overlay](../app/boards/rak4631_nrf52840.overlay) 叠在它上面。编完后的合并结果：`app/build/zephyr/zephyr.dts`。

行号按当前 145 行版本。

---

## 第 1–5 行：文件头

```c
/*
 * Copyright (c) 2021 Guillaume Paquet <guillaume.paquet@smile.fr>
 *
 * SPDX-License-Identifier: Apache-2.0
 */
```

版权和许可证。设备树编译器（dtc）当注释丢掉，不影响硬件描述。

---

## 第 7 行：`/dts-v1/;`

告诉 dtc：这是 Device Tree Source **版本 1**。Zephyr 板级 `.dts` 都必须有这一行。少了会当旧格式解析。

---

## 第 8–11 行：四个头文件

`#include` 和 C 一样，预处理时把文件原样插进来。`<>` 从 Zephyr 的 dts / include 搜索路径找；`""` 先从当前板目录找。

### 第 8 行：`#include <nordic/nrf52840_qiaa.dtsi>`

磁盘：`zephyr/dts/arm/nordic/nrf52840_qiaa.dtsi`。

**作用**：选定封装 **QIAA**（nRF52840 的一种封装），并再 include `nrf52840.dtsi`（整颗 SoC）。

里面有：CPU、`sram0`、`flash0`、`gpio0`/`gpio1`、`uart0`/`uart1`、`spi1`、`i2c0`/`i2c1`、`usbd`、`adc`、`wdt0`、`gpiote`、`uicr`、`reg0`/`reg1` 等。多数默认 `status = "disabled"`，本文件后面再用 `&xxx { status = "okay"; }` 打开。

没有这份，后面的 `&gpio1`、`&spi1`、`&uart1` 都没有可引用的节点。

### 第 9 行：`#include <nordic/nrf52840_partition.dtsi>`

磁盘：`zephyr/dts/vendor/nordic/nrf52840_partition.dtsi`。

**作用**：给 `flash0` 切分区，并设 `chosen`：

- `zephyr,sram` → `&sram0`
- `zephyr,flash` → `&flash0`
- `zephyr,code-partition` → `&slot0_partition`（应用程序放哪）

常见分区：`mcuboot`、`image-0`、`image-1`、storage。LBM 存上下文若用 `storage_partition`，也来自这类分区描述（具体名字以该 dtsi 为准）。

和 SX1262 无关，管的是 MCU 自己的 flash 布局。

### 第 10 行：`#include <zephyr/dt-bindings/lora/sx126x.h>`

磁盘：`zephyr/include/zephyr/dt-bindings/lora/sx126x.h`。

**作用**：给**树上** `semtech,sx1262` 驱动用的整数宏。本文件第 128 行的 `SX126X_DIO3_TCXO_3V3`（值为 `0x07`）就来自这里。

这不是 USP 那份。USP overlay 另 include：

```text
<zephyr/dt-bindings/usp/sx126x.h>   /* SX126X_TCXO_SUPPLY_*、SX126X_REG_MODE_* */
```

两套宏名字不同，对应两套 binding。

### 第 11 行：`#include "rak4631_nrf52840-pinctrl.dtsi"`

磁盘：同目录 [rak4631_nrf52840-pinctrl.dtsi](../zephyr/boards/rakwireless/rak4631/rak4631_nrf52840-pinctrl.dtsi)。

**作用**：把外设功能接到具体 pad。本 dts 里只写 `pinctrl-0 = <&uart1_default>`，真正的脚在这份 dtsi，例如：

| 符号 | 脚 |
|------|-----|
| `uart1_default` | TX P0.16，RX P0.15 |
| `spi1_default` | SCK P1.11，MOSI P1.12，MISO P1.13 |
| `uart0_default` | TX P0.20，RX P0.19 |
| `i2c0_default` | SDA P0.13，SCL P0.14 |

`_sleep` 组加了 `low-power-enable`，休眠时这些脚进低功耗。

---

## 第 13–45 行：根节点 `/ { ... }`

`/` 是整棵树的根。板级特有的、SoC dtsi 里没有的节点写在这里。

### 第 14 行：`model = "..."`

给人看的板子全名，日志 / `west boards` 会用。不参与驱动匹配。

### 第 15 行：`compatible = "nordic,rak4631_nrf52840"`

这块板的兼容字符串。一般给板级自己用，不是 SX1262 的 compatible。

### 第 17–23 行：`chosen { ... }`

Zephyr 的「全局选谁」。这里全部指到 `&uart1`：

| 属性 | 谁用 |
|------|------|
| `zephyr,console` | `printk` / LOG 默认串口 |
| `zephyr,shell-uart` | shell |
| `zephyr,uart-mcumgr` | mcumgr |
| `zephyr,bt-mon-uart` / `zephyr,bt-c2h-uart` | 蓝牙监视（本 app 没用蓝牙也无妨） |

overlay 会改 `console` / `shell-uart` 为 USB CDC。没改的几项仍是 uart1。

### 第 25–37 行：`leds`

`compatible = "gpio-leds"`：Zephyr LED 子系统。

- `blue_led`（标签 `led_2`）：P1.4，低有效  
- `green_led`（标签 `led_1`）：P1.3，低有效  

`xxx: yyy` 里 `xxx` 是标签（`&blue_led`），`yyy` 是节点名。

### 第 39–44 行：`aliases`

短名，C 里 `DT_ALIAS(led0)` 等。

| 别名 | 指向 | 谁认 |
|------|------|------|
| `led0` | 蓝灯 | 通用 LED sample |
| `lora0` | 下面的 `&lora` | **树上** Zephyr LoRa 驱动 / sample |
| `watchdog0` | `&wdt0`（来自 SoC dtsi） | watchdog 驱动 |

USP **不认** `lora0`。overlay 另加 `lora-transceiver` 和 `chosen { zephyr,lorawan-transceiver }`。

第 45 行 `};` 结束根节点。

---

## 第 47–74 行：打开 SoC 外设

语法 `&名字` = 给 dtsi 里已有节点补属性。

### 第 47–49 行：`&reg0`

高压稳压打开。nRF52840 从 USB/5V 供电时常要 REG0。

### 第 51–53 行：`&reg1`

核电压用 DCDC（比 LDO 省电）。`NRF5X_REG_MODE_DCDC` 来自 Nordic 的 dt-bindings（由 SoC dtsi 间接带入）。

### 第 55–57 行：`&adc`

ADC 打开。本 USP app 不一定用。

### 第 59–62 行：`&uicr`

- `gpio-as-nreset`：某脚当复位  
- `nfct-pins-as-gpios`：NFC 脚改当普通 GPIO（这块板不走 NFC）

### 第 64–66 行：`&gpiote`

GPIO 任务/事件（中断边沿等）。SX1262 的 DIO1 中断会用到 GPIOTE。

### 第 68–74 行：`&gpio0` / `&gpio1`

两个 GPIO 口打开。nRF52840 的 P0 / P1。SX1262 的复位、BUSY、CS、DIO、RX_EN 都在 **gpio1**。

---

## 第 76–92 行：两路 UART

### 第 76–83 行：`&uart0`

旧核 `nordic,nrf-uart`，115200，脚见 pinctrl P0.19/20。板级打开了，本 app 的 console 不走它。

### 第 85–92 行：`&uart1`

`nordic,nrf-uarte`（带 EasyDMA），115200，P0.15/16。板级默认 console。overlay 改 chosen 后，运行时打印走 USB，这两个节点仍在设备树里。

`pinctrl-names = "default", "sleep"` 必须和 `pinctrl-0` / `pinctrl-1` 两项对应。

---

## 第 94–109 行：I2C

### 第 94–100 行：`&i2c0`

TWI 打开，WisBlock 底板上的 I2C（P0.13/14）。和 SX1262 无关。

### 第 102–109 行：`&i2c1`

节点在，**没有** `status = "okay"`。注释写明：和 `spi1` 脚冲突。SX1262 占用 spi1，所以 i2c1 保持关闭。

---

## 第 111–132 行：SPI1 + SX1262（板级电台）

### 第 111–118 行：`&spi1`

打开 Nordic SPI。`cs-gpios = <&gpio1 10 GPIO_ACTIVE_LOW>`：片选 P1.10，低有效。时钟/MOSI/MISO 在 pinctrl：P1.11 / P1.12 / P1.13。

### 第 120–131 行：`lora: lora@0`

挂在 spi1 下的子设备。`lora@0` 的 `@0` 对应 `reg = <0>`（第 0 个片选，就是上面那个 CS）。

| 行 | 属性 | 含义 |
|----|------|------|
| 121 | `compatible = "semtech,sx1262"` | 绑 **Zephyr 自带** LoRa 驱动，不是 USP |
| 122 | `reg = <0>` | SPI 从设备地址 / CS 下标 |
| 123 | `reset-gpios` | P1.6，低有效复位 |
| 124 | `busy-gpios` | P1.14，高=忙 |
| 125 | `rx-enable-gpios` | P1.5，低=打开 RX 通路 |
| 126 | `dio1-gpios` | P1.15，IRQ |
| 127 | `dio2-tx-enable` | 芯片 DIO2 当 TX 开关（布尔，无值） |
| 128 | `dio3-tcxo-voltage` | DIO3 给 TCXO 的电压，宏来自第 10 行的头 |
| 129 | `tcxo-power-startup-delay-ms = <5>` | TCXO 稳定 5 ms |
| 130 | `spi-max-frequency` | 1 MHz |

overlay 会：改 `compatible` 为 `semtech,sx1262-new`；换成 USP 的 `dio2-as-rf-switch` / `dio3-as-tcxo-control` / `tcxo-voltage` / `reg-mode`；**删除** 125、127–129 以及 `label`。`reset` / `busy` / `reg` / SPI 父节点 overlay 不动。

---

## 第 134–139 行：`&qspi`

片外 QSPI flash（若模组有）。打开并指定 pinctrl。和 LoRa 无关。

---

## 第 141–144 行：USB

```dts
zephyr_udc0: &usbd {
	compatible = "nordic,nrf-usbd";
	status = "okay";
};
```

`&usbd` 引用 SoC 里的 USB 设备控制器。左边再打标签 `zephyr_udc0`，overlay 才能写 `&zephyr_udc0` 往下挂 CDC ACM。这里**只打开控制器**，没有串口功能；CDC 节点在 overlay 里。

---

## 和 overlay / 编译的关系

```text
nrf52840_qiaa.dtsi     SoC 节点
nrf52840_partition.dtsi   flash 分区
sx126x.h (树上)        SX126X_DIO3_TCXO_3V3
*-pinctrl.dtsi         引脚
rak4631_nrf52840.dts   本文件：打开外设 + 板上 SX1262
        ↓ 再叠
app/boards/rak4631_nrf52840.overlay
        ↓
app/build/zephyr/zephyr.dts
```

不要改本文件。`west update` 会按清单把 `zephyr/` 重置。板级差异只放 `app/` 的 overlay。
