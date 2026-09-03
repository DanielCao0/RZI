# rzi 文档

- [Docker 环境逐步说明](./zephyr-docker-environment-explained.md)：`app` 当老板、复用 4.4.99、接入 usp_zephyr 的每一步。
- [west 拓扑 T1 / T2 / T3](./west-topology.md)：清单仓是谁、本仓库为什么是 T2。
- [usp_zephyr 框架](./usp_zephyr-framework.md)：Semtech USP 分层、两个 git 仓库；架构图 [usp-zephyr-architecture.png](./usp-zephyr-architecture.png)。
- [三者关系](./usp-lbm-zephyr.md)：Zephyr、usp_zephyr、LBM 各管什么。
- [LBM 分层](./lbm-layers.md)：LoRa Basics Modem 方框图（应用 → 应用层包 → MAC → RAC）。
- [west patch](./west-patch.md)：Zephyr 官方补丁的目录约定、`patches.yml` 字段、命令用法。
- [RAK4631 板级设备树](./rak4631-dts.md)：`rak4631_nrf52840.dts` 逐行（四个 include、SX1262 节点）。
