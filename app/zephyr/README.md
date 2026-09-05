# 本仓库的 west patch

完整说明（官方目录约定、`patches.yml` 必填字段、校验和算法、命令）见 [doc/west-patch.md](../../doc/west-patch.md)。

本目录是清单仓里 west 默认读取的位置：`patches.yml` + `patches/`。这里的 `zephyr/` **不是** 工作区根的 Zephyr 源码。

```bash
./zephyr-docker.sh patch          # clean + apply
./zephyr-docker.sh patch-list
./zephyr-docker.sh build          # 编之前会自动打补丁
python3 app/zephyr/gen-patches-yml.py   # 扫描 patches/，覆盖 patches.yml
```

| path | 作用 |
|------|------|
| `usp_zephyr/0001-zephyr-4.4-warning-fixes.patch` | Zephyr 4.4 告警 |
| `usp_zephyr/0002-fix-lr-fhss-src-path.patch` | LR-FHSS 源码路径 |
| `usp_zephyr/0003-xiao-nrf54l15-full-name.patch` | Xiao 板 board.yml 补 full_name |
| `usp_zephyr/0004-sx1262-pa-compile-definitions.patch` | SX1262 型号宏传到 BSP，否则功放按 SX1261 配 |
