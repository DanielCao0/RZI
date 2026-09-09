# RZI FUOTA 与 ChirpStack

RZI 负责升级状态、镜像读取和可选的 MCUboot 安装。Clock Synchronization、
Remote Multicast Setup、Fragmentation 由选中的 backend 实现（USP / LBM，
或 Zephyr LoRaWAN services），不在 RZI 里复制。独立 GenAppKey 组播目前
只有 USP/LBM 路径完整。两种 backend 怎么编见
`doc/lorawan-backends.md`；Zephyr 合同限制见
`doc/lorawan-backend-zephyr.md`。

## 设备侧流程

应用负责 OTAA 入网、周期 uplink，并用 `rzi_fuota_register_callbacks()`
观察进度。协议与重组由 RZI 在 `CONFIG_RZI_LORAWAN_FUOTA=y` 时自动完成：

```text
OTAA join
  -> rzi_fuota_start()（lorawan_start 自动调用）
  -> 启动 ALCSync，并请求 MAC DeviceTime
  -> 周期 uplink（ChirpStack 靠它推进单播步骤）
  -> FPort 202 / 200 / 201 由 backend 透明处理
  -> Class C 组播收分片
  -> 记录镜像头，状态变为 COMPLETE
  -> 可选 rzi_fuota_apply() 写入 MCUboot secondary slot 并重启
```

OTAA 的 `application_key` 是 LoRaWAN 1.0.x **GenAppKey**，必须与 ChirpStack
设备上的 Gen App Key 一致。`network_key` 是 1.0.x AppKey。

## ChirpStack 配置

1. Device profile → Application layer 打开：
   - Application Layer Clock Synchronization **v1**
   - Remote Multicast Setup **v1**
   - Fragmented Data Block Transport **v1**
2. Expected uplink interval 设为 5–10 秒。ChirpStack 会按这个间隔检查
   单播步骤是否完成；间隔过长会表现为卡在 Multicast Setup。
3. 设备 OTAA 密钥填写 AppKey 和 Gen App Key。
4. 创建 FUOTA deployment：
   - Multicast group type: **Class C**
   - 让 ChirpStack 计算 fragment size 与 multicast timeout
   - 建议配置 fragmentation redundancy（例如 10–20%）
5. 上传要下发的文件。协议联调请用
   `samples/lorawan/fuota/test-payload/rzi-fuota-test.bin`（4096 字节，
   开头为 ASCII `RZI1`）。这不是可启动固件。若目标是 MCUboot 安装，
   再换成 signed image。

设备固件需选择同一套包版本：

```
CONFIG_RZI_LORAWAN_FUOTA=y
CONFIG_LORA_BASICS_MODEM_FUOTA_V1=y
```

若 ChirpStack 设备 profile 使用 v2，改为 `CONFIG_LORA_BASICS_MODEM_FUOTA_V2=y`。

ChirpStack 核心 FUOTA **不使用** Firmware Management Protocol（TS006）。
LBM 默认仍会编译 FMP；它只在网络服务器发送 FPort 203 时生效。

## 镜像容量

LBM 把重组后的镜像写到 context 分区的 `CONTEXT_FUOTA`（默认从 offset 4096
开始）。可用容量受该分区大小和 `FRAG_MAX_NB * FRAG_MAX_SIZE` 限制。

默认 sample 配置约为 200 × 100 = 20 KB，适合验证协议，不能放下完整
nRF52840 应用。要升级整包固件需要：

1. 调大 `CONFIG_LORA_BASICS_MODEM_FUOTA_MAX_NB_OF_FRAGMENTS`
2.    提供更大的 `lora-basics-modem-context-partition`，或打开
   `CONFIG_RZI_MCUBOOT` 后由 `rzi_fuota_apply()` 把镜像拷到 slot1
   （见 [boot.md](./boot.md)）

没有 `CONFIG_IMG_MANAGER` 时，`rzi_fuota_apply()` 返回 `-ENOTSUP`。
应用仍可用 `rzi_fuota_read_image()` 取出镜像。

## 公共 API

见 `include/rzi/fuota.h`：

- `rzi_fuota_register_callbacks()`
- `rzi_fuota_start()`
- `rzi_fuota_get_status()`
- `rzi_fuota_set_expected_size()`（可选覆盖；backend 会报重组长度，MCUboot 镜像也会自动测量）
- `rzi_fuota_read_image()`
- `rzi_fuota_apply()`

参考示例：`samples/lorawan/fuota`。
