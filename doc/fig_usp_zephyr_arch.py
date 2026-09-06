#!/usr/bin/env python3
"""usp_zephyr 分层架构图 → doc/usp-zephyr-architecture.png"""
from pathlib import Path
import sys

sys.path.insert(0, str(Path.home() / ".agents/skills/patent-diagram/scripts"))
from pillow_style import (  # noqa: E402
    Box,
    Canvas,
    EDGE,
    FACE,
    FILL,
    FILL_ON,
    FILL_SLEEP,
    HEADER,
    INK,
    MUTED,
)

OUT = Path(__file__).resolve().parent / "usp-zephyr-architecture.png"

# 层色：应用黄、协议绿、RAC 蓝绿、胶水蓝、内核灰蓝、硬件浅灰
APP = (255, 243, 205)
PROTO = (220, 237, 220)
RAC = (210, 232, 245)
GLUE = (236, 242, 249)
RTOS = (230, 230, 236)
HW = (245, 245, 245)
REPO = (252, 244, 228)


def main():
    W, H = 1180, 980
    c = Canvas(W, H)

    c.text_center((W // 2, 36), "usp_zephyr 架构", size=26, fill=HEADER)
    c.text_center(
        (W // 2, 68),
        "自上而下调用。中间黄/绿在 usp 仓，蓝框胶水在 usp_zephyr 仓。",
        size=13,
        fill=MUTED,
    )

    # 左：调用栈    右：对应 git
    lx0, lx1 = 40, 820
    rx0, rx1 = 850, 1140
    y = 100
    gap = 14
    h_app, h_proto, h_rac, h_glue, h_rtos, h_hw = 88, 100, 88, 150, 80, 80

    app = Box(lx0, y, lx1, y + h_app)
    y = app.y1 + gap
    proto = Box(lx0, y, lx1, y + h_proto)
    y = proto.y1 + gap
    rac = Box(lx0, y, lx1, y + h_rac)
    y = rac.y1 + gap
    glue = Box(lx0, y, lx1, y + h_glue)
    y = glue.y1 + gap
    rtos = Box(lx0, y, lx1, y + h_rtos)
    y = rtos.y1 + gap
    hw = Box(lx0, y, lx1, y + h_hw)

    c.rounded_box(app, "", fill=APP, fontsize=14)
    c.text_center((app.cx, app.y0 + 28), "你的应用", size=18)
    c.text_center(
        (app.cx, app.y0 + 58),
        "samples/lorawan/counter/src/main.c    smtc_modem_init / request_uplink    overlay 描述引脚",
        size=13,
        fill=MUTED,
    )

    # 协议层拆两格
    p1 = Box(proto.x0 + 16, proto.y0 + 36, proto.x0 + 390, proto.y1 - 12)
    p2 = Box(proto.x0 + 406, proto.y0 + 36, proto.x1 - 16, proto.y1 - 12)
    c.rounded_box(proto, "", fill=PROTO)
    c.text_center((proto.cx, proto.y0 + 20), "协议（平台无关，在 usp 仓）", size=16)
    c.rounded_box(p1, "LBM  LoRaWAN\njoin / MAC / 区域", fill=FILL_ON, fontsize=14)
    c.rounded_box(p2, "其它协议（可扩展）\n测距 / CAD / SDK", fill=FACE, fontsize=13)

    c.rounded_box(rac, "", fill=RAC)
    c.text_center((rac.cx, rac.y0 + 28), "RAC  Radio Access Controller", size=18)
    c.text_center(
        (rac.cx, rac.y0 + 58),
        "usp/smtc_rac_lib    排队占用电台    定时 / ASAP / 优先级",
        size=13,
        fill=MUTED,
    )

    g1 = Box(glue.x0 + 16, glue.y0 + 40, glue.x0 + 265, glue.y1 - 14)
    g2 = Box(glue.x0 + 281, glue.y0 + 40, glue.x0 + 530, glue.y1 - 14)
    g3 = Box(glue.x0 + 546, glue.y0 + 40, glue.x1 - 16, glue.y1 - 14)
    c.rounded_box(glue, "", fill=GLUE)
    c.text_center((glue.cx, glue.y0 + 22), "usp_zephyr 胶水（只认识 Zephyr）", size=16)
    c.rounded_box(g1, "MCU HAL\nsmtc_modem_hal\nk_uptime / flash", fill=FILL, fontsize=13)
    c.rounded_box(g2, "Radio HAL\ndrivers/usp\nsx126x SPI / DIO", fill=FILL, fontsize=13)
    c.rounded_box(g3, "板级 dts\nboards / shields\nRAK 用 app overlay", fill=FILL, fontsize=13)

    c.rounded_box(rtos, "", fill=RTOS)
    c.text_center((rtos.cx, rtos.y0 + 26), "Zephyr 内核", size=18)
    c.text_center(
        (rtos.cx, rtos.y0 + 54),
        "线程 / SPI / GPIO / USB CDC / flash    不认识 LBM",
        size=13,
        fill=MUTED,
    )

    c.rounded_box(hw, "", fill=HW, outline=INK)
    c.text_center((hw.cx, hw.y0 + 26), "硬件", size=18)
    c.text_center((hw.cx, hw.y0 + 54), "nRF52840  +  SX1262（RAK4631 板上焊死）", size=13, fill=MUTED)

    for a, b in ((app, proto), (proto, rac), (rac, glue), (glue, rtos), (rtos, hw)):
        c.v_link(a, b)

    # 右侧仓库对照
    c.text_center(((rx0 + rx1) // 2, 100), "对应 git / 目录", size=15, fill=HEADER)

    def repo_box(y0, y1, title, body, fill=REPO):
        b = Box(rx0, y0, rx1, y1)
        c.rounded_box(b, "", fill=fill)
        c.text_center((b.cx, b.y0 + 22), title, size=14)
        c.text_center((b.cx, b.cy + 10), body, size=12, fill=MUTED)
        return b

    r_app = repo_box(app.y0, app.y1, "app/", "你改\nwest 清单仓")
    r_usp = repo_box(proto.y0, rac.y1, "modules/lib/usp/", "不改\nLBM + RAC")
    r_uz = repo_box(glue.y0, glue.y1, "usp_zephyr/", "不改（补丁除外）\nHAL + 驱动 + sample")
    r_z = repo_box(rtos.y0, rtos.y1, "zephyr/", "不改")
    r_hw = repo_box(hw.y0, hw.y1, "芯片", "RAK4631", fill=HW)

    # 横线把层和仓库对齐（视觉提示，短线）
    for left, right in (
        (app, r_app),
        (proto, r_usp),
        (glue, r_uz),
        (rtos, r_z),
        (hw, r_hw),
    ):
        mid = (left.cy + right.cy) // 2 if abs(left.cy - right.cy) < 8 else left.cy
        c.ortho([(left.x1, left.cy), (right.x0, mid)])

    c.text_center(
        (W // 2, H - 22),
        "调用：app → LBM → RAC → usp_zephyr HAL → Zephyr SPI/GPIO → SX1262",
        size=13,
        fill=MUTED,
    )

    c.save(OUT)
    print(OUT)


if __name__ == "__main__":
    main()
