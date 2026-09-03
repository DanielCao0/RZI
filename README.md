# rzi Zephyr

`app/` 当 west 清单（`app/west.yml`）。Zephyr 用当前树（4.4.99）；`usp_zephyr` / `usp` 跟 main，不 import 官方那份 v4.2.0 清单。

```bash
chmod +x zephyr-docker.sh
./zephyr-docker.sh build-image   # 只需一次
./zephyr-docker.sh init          # 首次：west init -l app + west update
./zephyr-docker.sh build         # 编 app/（rak4631）；编前会 west patch apply
./zephyr-docker.sh patch         # 按 app/zephyr/patches.yml 打官方补丁
./zephyr-docker.sh sample        # 编 usp_zephyr 官方 periodical_uplink
./zephyr-docker.sh shell
```

`sample` 用的是 **nRF52840 DK + Semtech SX126x 盾**，不是 RAK4631。

逐步说明（为什么这样写清单、怎么挪树、Docker 怎么挂）见 [doc/zephyr-docker-environment-explained.md](./doc/zephyr-docker-environment-explained.md)。T1 / T2 / T3 见 [doc/west-topology.md](./doc/west-topology.md)。USP / LBM 笔记见 [doc/README.md](./doc/README.md)。给 `usp_zephyr` 打补丁用官方 `west patch`，见 [doc/west-patch.md](./doc/west-patch.md)。
