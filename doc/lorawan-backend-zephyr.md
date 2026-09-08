# Zephyr LoRaWAN API backend

`CONFIG_RZI_LORAWAN_BACKEND_ZEPHYR` 把 RZI 接到 Zephyr 公共
`<zephyr/lorawan/lorawan.h>`，下层固定为 **loramac-node**（不是 native，
也不是 LoRa Basics Modem radio backend）。这和规划中的 Zephyr LBM
LoRaWAN backend（`CONFIG_RZI_LORAWAN_BACKEND_ZEPHYR_LBM`）不是同一条路。

实现这个 adapter 是为了用第二套真实栈检验 RZI 合同是否合理。结论：
**异步 join/send、事件 sink、FUOTA 扩展表是站得住的**；有几处字段和能力
被 LBM 形状带偏，已经在公共头里补注释或小字段，其余由 backend 消化。

## 合同能适配的部分

| RZI 合同 | Zephyr API | Adapter |
| --- | --- | --- |
| `join()` / `send()` 立即返回，完成走事件 | 两个调用都阻塞到结束 | 专用 work queue |
| `READY` / `JOINED` / `JOIN_FAILED` / `TX_DONE` / `DOWNLINK` | 无对等事件，成功看返回值 | worker 结束后发布事件 |
| `start_clock_sync` / 镜像读写 / reboot | `lorawan_clock_sync_run()`、slot1 flash、`sys_reboot()` | FUOTA ops 扩展 |
| 运行时 `set_region()` | `lorawan_set_region()`，且 region 还受 Kconfig 裁剪 | 映射失败则 `start()` 失败 |

## 第二套 backend 暴露的摩擦

1. **没有 `leave()`**。
   Zephyr 不能结束会话。backend 返回 `-ENOTSUP`，服务状态保持 JOINED。
2. **没有 `is_joined()`**。
   Adapter 只记录自己发起的 join 结果。NVM 恢复的会话不会自动变成已入网。
3. **DevNonce 由应用持有**。
   `LORAWAN_NVM_NONE` 时 Zephyr 要求每次 OTAA 提供单调递增的 nonce。RZI
   现在有可选的 `otaa.dev_nonce`；0 表示 backend 自增（掉电丢失）。
4. **密钥命名是 1.1 的，语义却叠了 1.0 GenAppKey**。
   RZI `network_key` 对应 Zephyr `nwk_key`（1.0 AppKey / 1.1 NwkKey）。
   RZI `application_key` 对应 Zephyr `app_key`（1.1 AppKey）。
   LoRaWAN 1.0.x 的 **GenAppKey 不是 Zephyr join 参数**。ChirpStack 组播
   若要求独立 GenAppKey，请继续用 USP/LBM，或把两个 RZI 字段都填 AppKey。
5. **`join_backoff_bypass` 被忽略**。
   Zephyr 没有 join duty-cycle bypass。
6. **FUOTA 事件不完整**。
   无组播 session start/end 公共回调；用 FragSession descriptor 近似
   `SESSION_STARTED`。`lorawan_frag_transport_run()` 的结束回调没有成功
   /失败和镜像长度。无 FMP，因此没有 `REBOOT_REQUESTED`。镜像长度返回
   `-ENODATA`，由 RZI 从 MCUboot 头推断。
7. **AS923 只有一个 region**。
   RZI 的 GRP1–4 都映射到 `LORAWAN_REGION_AS923`。
8. **Class B 不支持**。
   `set_class(B)` 返回 `-ENOTSUP`。
9. **JOIN_FAILED 现在带 errno**。
   Zephyr `lorawan_join()` 返回具体错误。服务不再一律报 `-ETIMEDOUT`。

## 使用

完整的 USP / Zephyr 编译命令、overlay 替换和 `.config` 检查见
[编译两种 LoRaWAN backend](./lorawan-backends.md)。

```text
CONFIG_RZI_LORAWAN=y
CONFIG_RZI_LORAWAN_BACKEND_ZEPHYR=y
CONFIG_LORA_MODULE_BACKEND_LORAMAC_NODE=y
CONFIG_LORAWAN_REGION_EU868=y
```

应用仍只调用 `rzi_lorawan_*`。必须同时打开对应的 Zephyr region Kconfig，
并保证板级 DTS 有 `lora0`。FUOTA 还需要 `CONFIG_LORAWAN_SERVICES`、
clock sync、fragmentation，以及可选的 remote multicast。
