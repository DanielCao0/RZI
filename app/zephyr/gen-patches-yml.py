#!/usr/bin/env python3
"""扫描 patches/**/*.patch，覆盖写入 patches.yml（west 算法算 sha256）。

用法（在仓库根或本目录均可）：

  python3 app/zephyr/gen-patches-yml.py

已有同 path 的 comments / author / email / date 会保留，只更新 sha256。
"""
from __future__ import annotations

import argparse
import hashlib
import re
from datetime import date
from pathlib import Path

HERE = Path(__file__).resolve().parent
PATCH_ROOT = HERE / "patches"


def west_sha256(path: Path) -> str:
    with open(path, encoding="utf-8", newline=None) as fp:
        return hashlib.sha256(fp.read().encode("utf-8")).hexdigest()


def load_old(yml: Path) -> dict[str, dict]:
    """按 path 记住旧项里的人工字段。"""
    if not yml.is_file():
        return {}
    text = yml.read_text(encoding="utf-8")
    old: dict[str, dict] = {}
    current: dict | None = None
    in_comments = False
    comment_lines: list[str] = []
    for line in text.splitlines():
        m = re.match(r"  - path: (\S+)", line)
        if m:
            if current:
                if comment_lines:
                    current["comments"] = "\n".join(comment_lines).rstrip()
                old[current["path"]] = current
            current = {"path": m.group(1)}
            in_comments = False
            comment_lines = []
            continue
        if current is None:
            continue
        if in_comments:
            if line.startswith("      "):
                comment_lines.append(line[6:])
                continue
            in_comments = False
        km = re.match(r"    (author|email|date|upstreamable): (.+)$", line)
        if km:
            current[km.group(1)] = km.group(2)
        elif re.match(r"    comments: \|", line):
            in_comments = True
    if current:
        if comment_lines:
            current["comments"] = "\n".join(comment_lines).rstrip()
        old[current["path"]] = current
    return old


def yaml_comment_block(text: str) -> list[str]:
    return ["    comments: |"] + [f"      {ln}" if ln else "      " for ln in text.split("\n")]


def main() -> None:
    p = argparse.ArgumentParser(description="从 patches/ 覆盖生成 patches.yml")
    p.add_argument("-o", "--output", type=Path, default=HERE / "patches.yml")
    p.add_argument("--author", default="rzi")
    p.add_argument("--email", default="patches@rzi.local")
    args = p.parse_args()

    files = sorted(PATCH_ROOT.rglob("*.patch"))
    if not files:
        raise SystemExit(f"no .patch under {PATCH_ROOT}")

    old = load_old(args.output)

    lines = [
        "# 由 gen-patches-yml.py 扫描 patches/ 生成。west patch 读本文件。",
        "# 文档：doc/west-patch.md",
        "checkout-command: git checkout .",
        'clean-command: ""',
        "",
        "patches:",
    ]
    for f in files:
        rel = f.relative_to(PATCH_ROOT).as_posix()
        module = rel.split("/", 1)[0]
        prev = old.get(rel, {})
        stamp = prev.get("date") or date.fromtimestamp(f.stat().st_mtime).isoformat()
        comments = prev.get("comments") or f"从 {rel} 自动登记（comments 需手改）。"
        lines += [
            f"  - path: {rel}",
            f"    sha256sum: {west_sha256(f)}",
            f"    module: {module}",
            f"    author: {prev.get('author', args.author)}",
            f"    email: {prev.get('email', args.email)}",
            f"    date: {stamp}",
            f"    upstreamable: {prev.get('upstreamable', 'true')}",
            "    apply-command: git apply",
            *yaml_comment_block(comments),
            "",
        ]

    args.output.write_text("\n".join(lines).rstrip() + "\n", encoding="utf-8")
    print(f"wrote {args.output} ({len(files)} patches)")


if __name__ == "__main__":
    main()
