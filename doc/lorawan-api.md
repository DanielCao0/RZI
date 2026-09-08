# RZI LoRaWAN 公共 API 规范

## 1. 目标

RZI LoRaWAN API 是应用与具体协议栈之间稳定的 C 接口。公共接口遵循以下原则：

- 操作形态尽量接近 Zephyr LoRaWAN API，降低 Zephyr 开发者的迁移成本；
- 异步结果采用 RUI3 风格 callback，而不是让应用轮询内部队列；
- 公共类型不暴露 USP、LoRa Basics Modem 或其他 backend 类型；
- 一个 backend、一个 modem 实例，但允许多个相互独立的 callback 订阅者；
- 不使用堆内存，所有跨线程数据由 RZI 复制；
- API 返回值只表示请求是否被接受，最终结果由 callback 报告。

本文档是 LoRaWAN 公共 API 的权威规范；头文件
`include/rzi/lorawan.h` 是当前核心 ABI 的精确定义。

Class B、network/channel management 和 information 属于 LoRaWAN Core，
不建立空的独立服务目录。DeviceTimeReq 与 LinkCheckReq 统一保留在
`src/lorawan/mac_commands/`。RUI3 确实独立实现的 channel scan、multicast、
certification 和 FUOTA 放在 `src/lorawan/services/`。FUOTA 所需的 Clock
Synchronization、Remote Multicast Setup、Fragmentation 和 Firmware
Management Package 由具体 backend 提供；RZI 不复制协议包实现。

这些内部 header 的版本为 `0.0.0`，不进入公共 include 路径，也不代表 backend
具备对应 capability。新能力只有在公共 API、backend contract、Kconfig、测试和
文档同时完成后，才新增 `include/rzi/lorawan/<feature>.h` facade。

RUI3 的 LoRa P2P/FSK 能力不属于 LoRaWAN，RZI 将其保留在独立的
`src/lora/` 内部边界。完整映射见 `doc/rui3-c-service-framework.md`。

## 2. 与 Zephyr 和 RUI3 的关系

RZI 保留 Zephyr 的主要操作顺序和概念：

```text
set_region -> start -> join(config) -> send(port, data, size, type)
```

- `join` 接收独立的 activation config；
- `send` 使用 confirmed/unconfirmed message type；
- region、activation、device class 和 message type 都是显式 enum；
- backend 不支持的标准能力返回 `-ENOTSUP`。

RZI 不照搬 Zephyr 的同步 join 语义。USP/LBM 的 join、TX 和 downlink 本来就是
异步事件，因此 RZI 使用 RUI3 风格的 `join_done`、`send_done`、`downlink` 和
`state_changed` callback。这样不会在调用线程中长时间阻塞，也不会伪造同步
结果。

## 3. 生命周期

典型调用顺序：

```c
static const struct rzi_lorawan_callbacks callbacks = {
    .join_done = on_join_done,
    .send_done = on_send_done,
    .downlink = on_downlink,
    .state_changed = on_state_changed,
    .error = on_error,
    .user_data = context,
};

rzi_lorawan_register_callbacks(&callbacks, &handle);
rzi_lorawan_set_region(RZI_LORAWAN_REGION_EU_868);
rzi_lorawan_start();
/* state_changed(RZI_LORAWAN_STATE_READY) 后 */
rzi_lorawan_join(&join_config);
```

规则：

1. callback 可以在 `start` 前注册，推荐这样做，避免错过 `READY` state；
2. `set_region` 和 `set_join_backoff_bypass` 只能在 `start` 前调用；
3. `start` 是进程级一次性操作；重复调用返回 `-EALREADY`；
4. `RZI_LORAWAN_STATE_READY` 表示 backend 可接受 join，不表示设备已入网；
5. modem 意外 reset 后，RZI 会重新配置 backend 并再次发出 `READY` state；
6. 当前版本没有 stop/reinit，backend 启动后的致命错误应由应用记录并重启设备。

## 4. Activation

`rzi_lorawan_join_config` 的结构与 Zephyr `lorawan_join_config` 对齐，使用
activation enum 和 union：

- OTAA：`dev_eui`、`join_eui`、`network_key`、`application_key`；
- ABP：`dev_addr`、`network_session_key`、`application_session_key`。

公共 API 可以表达 OTAA 和 ABP，但具体支持由 capability 决定。应用应通过
`rzi_lorawan_get_capabilities()` 检查能力。当前 USP backend 支持 OTAA 和
Class A；ABP、Class B、Class C 返回 `-ENOTSUP`。

join config 在 `rzi_lorawan_join()` 返回前完成深拷贝，调用者之后可以释放或
修改原对象。返回 `0` 仅表示请求已被 backend 接受。`join_done` 的 status 为
`0` 表示网络激活完成，负 errno 表示本次尝试结束但未入网。

重试策略属于应用或 AT service，RZI core 不进行隐藏的无限重试。

## 5. Uplink 和 downlink

`rzi_lorawan_send()` 的 port 范围为 1 到 223，最大 payload 为
`RZI_LORAWAN_MAX_PAYLOAD`。payload 在函数返回前由 backend 复制。一次只允许
一个未完成 uplink；已有请求时返回 `-EBUSY`。

`send_done` 收到只在 callback 期间有效的 `rzi_lorawan_tx_result`：

- `RZI_LORAWAN_TX_ACKED`：confirmed uplink 收到确认；
- `RZI_LORAWAN_TX_SENT`：帧已发送，但没有确认；
- `RZI_LORAWAN_TX_NOT_SENT`：请求最终未发送。

downlink callback 收到的 metadata 和 payload 指针只在本次 callback 返回前
有效。需要延后处理时，订阅者必须复制数据。

## 6. Callback 与并发模型

backend event callback 只把完整事件复制到固定容量的 RZI message queue，然后
立即返回。RZI dispatcher thread 从队列取出事件，并按注册顺序串行调用所有
订阅者：

```text
USP/LBM callback -> private event queue -> RZI dispatcher -> subscribers
```

这保证用户 callback：

- 不在 ISR 中执行；
- 不在 USP/LBM `rac_api_mutex` 内执行；
- 不并发执行同一个事件的多个订阅者；
- 可以安全调用非阻塞的 RZI LoRaWAN API。

callback 必须尽快返回，不能进行长时间阻塞。一个慢订阅者会延迟其他订阅者；
队列溢出时，RZI 通过 `error(-EOVERFLOW)` 报告丢失。队列深度、订阅者上限、
dispatcher stack 和 priority 均由 Kconfig 配置。

callback table（包括其中的 `user_data`）在注册时复制。`user_data` 指向的
对象仍由调用者拥有。注销会阻止新的
dispatch snapshot 包含该订阅者，但注销时已经开始的 callback 可能仍在执行；
调用者必须在所有 in-flight callback 返回后再释放 `user_data`。

## 7. 错误语义

所有可失败 API 返回 `0` 或负 errno：

- `-EINVAL`：参数、enum、port 或 payload 长度无效；
- `-EWOULDBLOCK`：从 ISR 调用了 thread-context API；
- `-EAGAIN`：service/backend 尚未 ready；
- `-EALREADY`：重复 start，或在 start 后修改启动配置；
- `-EBUSY`：已有冲突中的异步操作；
- `-ENOTSUP`：selected backend 不支持请求能力；
- `-ENOMEM`：callback subscriber slots 已满；
- `-EIO`：backend 未提供更精确错误的底层失败。

`error` callback 用于异步 backend 错误和 event queue overflow，不替代同步 API
参数检查。

## 8. Backend contract

私有 `lorawan_backend.h` 定义 backend vtable 和内部 event envelope。backend：

- 提供 capability bit mask；
- 在 `start` 时接收 region、开发策略和 event sink；
- 深拷贝 join config 和 send payload；
- 将供应商返回值转换为负 errno；
- 不直接调用用户 callback；
- 不把供应商对象、enum 或线程模型泄漏到公共头文件。

`lorawan_feature.h` 定义可选能力的 versioned extension descriptor。每个真实
feature 在自己的私有头中定义 typed operations table，并通过 feature ID 从
backend 获取；`size` 和 `version` 用于兼容性校验。公共 capability bit 表示
运行时支持，Kconfig 只表示该 feature facade 被编译，二者不能互相替代。

新增 backend 只需实现 core contract，并按实际能力提供 extension；不应要求
应用修改公共调用流程。

## 9. 兼容性策略

本次接口替换早期 v0.1 的 `rzi_lorawan_init(config)` 和
`rzi_lorawan_get_event()`。RZI 尚未发布稳定 1.0 ABI，因此不保留容易造成双
事件消费者和语义歧义的兼容 wrapper。后续公共 ABI 的破坏性修改必须：

1. 更新本文档和公共头文件；
2. 更新所有 RZI samples、AT service 和集成应用；
3. 通过格式检查、单元测试和至少一个真实 backend 构建；
4. 在 release note 中明确迁移方法。
