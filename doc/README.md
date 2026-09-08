# RZI 文档

- [RZI SDK 总体架构](./rzi-sdk-architecture.md)：正式的分层、职责边界、后端策略、依赖策略以及 AT、NVM、功耗、FUOTA 和 Arduino/RUI 路线图。
- [RZI LoRaWAN 公共 API 规范](./lorawan-api.md)：融合 Zephyr 操作形态与 RUI3 异步 callback 体验的生命周期、并发、错误和 backend contract。
- [编译两种 LoRaWAN backend](./lorawan-backends.md)：USP/LBM 与 Zephyr `lorawan_*` + loramac-node 的 Kconfig、overlay 和 west 命令。
- [Zephyr LoRaWAN API backend](./lorawan-backend-zephyr.md)：第二套真实栈对 RZI 合同的验证结论。
- [RZI FUOTA 与 ChirpStack](./fuota.md)：协议归属、设备流程、ChirpStack 配置和镜像容量限制。
- [RZI Storage API](./storage-api.md)：命名空间、错误语义、AT settings 迁移与后续安全存储边界。
- [RZI 公共 API 注解与文档规范](./api-documentation-guidelines.md)：Doxygen、版本、线程与生命周期说明，以及 must-check、printf-like 和 deprecated 编译器检查要求。
- [RZI 文件与函数命名规范](./naming-conventions.md)：所有新增和修改代码必须遵循的文件、公共 API、私有函数、AT 命令与构建符号命名规则。
- [RZI AT 框架](./at-framework.md)：AT core、命令注册、可选命令包和 I/O adapter 的边界。
- [RZI AT 指令与 RUI3 兼容性](./at-command-compatibility.md)：已实现指令、参数语义、异步事件、持久化及完整 RUI3 指令集的差异。
- [RUI3 C service 框架映射](./rui3-c-service-framework.md)：RUI3 LoRa C service、标准 packages、P2P 与 AT command domains 在 RZI 中的完整占位边界。
- [RZI 与 Zephyr 集成重构](./rzi-zephyr-refactoring-guide.md)：目录、公共 API、后端边界和验证结果。
- [usp_zephyr 框架](./usp_zephyr-framework.md)：Semtech USP 分层和两个 Git 仓库。
- [三者关系](./usp-lbm-zephyr.md)：Zephyr、usp_zephyr、LBM 各自负责什么。
- [LBM 分层](./lbm-layers.md)：应用、应用层包、MAC 与 RAC 的关系。
- [RAK4631 板级设备树](./rak4631-dts.md)：上游 SX1262 节点和 sample 兼容配置的历史分析。

`rzi-sdk-architecture.md` 是当前架构的权威文档。其余文档记录早期调研和
USP 后端实现背景；涉及 RZI 自带 DTS、板级源码或 Zephyr 尚未集成 LBM 的
描述可能已经过时，应以总体架构和当前代码为准。
