# ChirpStack FUOTA 测试文件

`rzi-fuota-test.bin`：4096 字节，小于当前 sample 的 20 KB 上限。

| 偏移 | 内容 |
|------|------|
| 0–3 | ASCII `RZI1` |
| 4–7 | 小端长度 `4096` |
| 8 起 | `ChirpStack FUOTA test payload for RZI` |

在 ChirpStack FUOTA deployment 里上传这个文件，不要传完整应用固件。
成功后看 `rzi_fuota` 日志里的 `FUOTA image head`，开头应为 `52 5a 49 31`（`RZI1`）。

重新生成：

```bash
python3 samples/lorawan/fuota/test-payload/gen_test_bin.py
```
