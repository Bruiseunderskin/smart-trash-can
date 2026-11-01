#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""garbage265 → 四類 ImageFolder，供 Edge Impulse / PyTorch 直接使用。

用途
----
把大型垃圾分類資料集（例如 ModelScope 的 `tany0699/garbage265`，4 大類 / 265 小類）
整理成香港四桶的四類結構：可回收物 / 廚餘 / 有害 / 其他。

輸出
----
<out>/
  train/<類別>/*.jpg
  val/<類別>/*.jpg
  test/<類別>/*.jpg
  labels.csv          每一張圖的來源、目標類別、split
  mapping.csv         每個來源類別 → 目標類別、張數
  report.md           完整報告（含未映射清單、類別平衡、重複檔）

設計原則
--------
1. 唔假設資料集結構。自動偵測「一層類別目錄」或「train/val/test 三層」。
2. 唔假設類別名。先用 mapping.json，再用關鍵詞啟發式，最後列出 UNMAPPED 交人決定。
3. 唔會靜靜丟資料。每一張圖的去向都寫入 labels.csv，未映射的全部列在 report.md。
4. 去重先行，避免同一張圖出現在 train 同 test。

用法
----
    # 1) 已下載到本機
    python tools/build_dataset.py --src ~/datasets/garbage265 --out data/waste4

    # 2) 由 ModelScope 下載（需要 modelscope 套件）
    python tools/build_dataset.py --download --cache ~/datasets/garbage265 \
        --out data/waste4 --per-class-limit 1500

    # 3) 帶自訂映射
    python tools/build_dataset.py --src <dir> --mapping configs/waste4_map.json --out data/waste4

    # 4) 順便出 Edge Impulse 上傳用的 zip（每個類別一個資料夾）
    python tools/build_dataset.py --src <dir> --out data/waste4 --ei-zip data/waste4_ei.zip
"""

from __future__ import annotations

import argparse
import csv
import hashlib
import json
import os
import random
import shutil
import sys
import zipfile
from collections import Counter, defaultdict
from pathlib import Path

IMG_EXT = {".jpg", ".jpeg", ".png", ".bmp", ".webp", ".jfif"}

TARGETS = ["可回收物", "廚餘", "有害", "其他"]

# 關鍵詞啟發式：只覆蓋最常見的叫法。沒有 hit 的一律進 UNMAPPED，
# 由 report.md 列出，再由人補 mapping.json。
KEYWORDS = {
    "可回收物": [
        "塑料", "塑胶", "塑料瓶", "瓶", "罐", "易拉罐", "饮料瓶", "矿泉水",
        "纸", "纸箱", "纸板", "报纸", "书籍", "书本", "快递盒", "纸盒",
        "金属", "铁", "铝", "不锈钢", "罐头",
        "玻璃", "玻璃瓶", "酒瓶",
        "衣物", "衣服", "布料", "织物", "纺织品", "鞋", "包",
        "家电", "电器", "电子产品", "充电器", "数据线",
        "bottle", "can", "paper", "cardboard", "metal", "glass", "plastic",
        "carton", "newspaper", "clothes", "textile",
    ],
    "廚餘": [
        "厨余", "厨馀", "果皮", "果核", "剩饭", "剩菜", "菜叶", "菜梗",
        "食物", "食材", "骨头", "鱼刺", "蛋壳", "茶叶", "咖啡渣", "面包",
        "水果", "蔬菜", "肉类", "米饭", "面条", "坚果", "瓜子",
        "food", "organic", "vegetable", "fruit", "meat", "bone",
    ],
    "有害": [
        "电池", "蓄电池", "纽扣电池", "灯管", "荧光灯", "节能灯", "灯泡",
        "药品", "药片", "过期药", "药瓶", "医疗",
        "油漆", "涂料", "杀虫剂", "农药", "除草剂", "消毒剂",
        "温度计", "水银", "化学品", "溶剂", "胶水", "过期化妆品", "指甲油",
        "battery", "bulb", "medicine", "paint", "pesticide", "chemical",
    ],
    "其他": [
        "纸巾", "卫生纸", "厕纸", "尿布", "尿不湿", "口罩", "烟头", "烟蒂",
        "陶瓷", "花盆", "尘土", "灰土", "一次性餐具", "牙签", "创可贴",
        "胶带", "保鲜膜", "吸管", "湿巾", "猫砂", "头发",
        "tissue", "diaper", "mask", "cigarette", "ceramic", "trash",
    ],
}


def eprint(*a):
    print(*a, file=sys.stderr)


def is_image(p: Path) -> bool:
    return p.suffix.lower() in IMG_EXT


def iter_images(root: Path):
    for dirpath, _dirnames, filenames in os.walk(root):
        for fn in filenames:
            p = Path(dirpath) / fn
            if is_image(p):
                yield p


def detect_layout(src: Path) -> tuple[str, dict[str, list[Path]]]:
    """回傳 (layout, {類別名: [檔案]})。

    layout 可能是：
      "flat_classes"  — 一層類別目錄（最常見）
      "split_dirs"    — train/val/test 之下再分類別
      "unknown"
    """
    subs = [d for d in src.iterdir() if d.is_dir()]
    if not subs:
        return "unknown", {}

    split_names = {"train", "training", "val", "valid", "validation", "test", "testing"}
    if {d.name.lower() for d in subs} <= split_names and len(subs) >= 2:
        classes: dict[str, list[Path]] = defaultdict(list)
        for d in subs:
            for sub in d.iterdir():
                if sub.is_dir():
                    classes[sub.name].extend(p for p in iter_images(sub))
                else:
                    classes[d.name].append(sub) if is_image(sub) else None
        return "split_dirs", dict(classes)

    classes = {}
    for d in subs:
        imgs = list(iter_images(d))
        if imgs:
            classes[d.name] = imgs
    if classes:
        return "flat_classes", classes

    return "unknown", {}


def load_mapping(path: Path | None) -> dict[str, str]:
    if not path:
        return {}
    data = json.loads(path.read_text(encoding="utf-8"))
    # 支援 {"map": {...}} 或直接 {...}
    if isinstance(data, dict) and "map" in data and isinstance(data["map"], dict):
        data = data["map"]
    bad = {k: v for k, v in data.items() if v not in TARGETS}
    if bad:
        raise SystemExit(f"mapping 有無效目標類別：{bad}（必須是 {TARGETS}）")
    return data


def keyword_target(name: str) -> str | None:
    """最長匹配優先。例如「紙巾」同時命中「紙」（可回收）與「紙巾」（其他），
    取較長的「紙巾」→ 其他，避免子字串造成的錯誤分類。"""
    low = name.lower()
    hits: list[tuple[int, str]] = []
    for target, words in KEYWORDS.items():
        for w in words:
            if w.lower() in low:
                hits.append((len(w), target))
    if not hits:
        return None
    hits.sort(key=lambda x: -x[0])
    best_len, best_target = hits[0]
    # 同長度但指向不同類別 = 真衝突，交人決定
    if len(hits) > 1 and hits[1][0] == best_len and hits[1][1] != best_target:
        return None
    return best_target


def file_md5(p: Path, chunk: int = 1 << 20) -> str:
    h = hashlib.md5()
    with p.open("rb") as f:
        while True:
            b = f.read(chunk)
            if not b:
                break
            h.update(b)
    return h.hexdigest()


def stratified_plan(items, seed: int, ratio: tuple[float, float, float]) -> dict[tuple[str, str], str]:
    """每個目標類別內部按雜湊排序，再按比例切分。

    保證：
      - 可重現（同一 seed + 同一檔案名 → 同一 split）
      - 每個類別的 train/val/test 比例貼近設定值（唔會出現 val 只有 1 張）
    """
    buckets: dict[str, list] = defaultdict(list)
    for it in items:
        buckets[it[2]].append(it)
    plan: dict[tuple[str, str], str] = {}
    for target, lst in buckets.items():
        order = sorted(lst, key=lambda it: hashlib.md5(f"{seed}:{it[1]}/{it[0].name}".encode()).hexdigest())
        n = len(order)
        n_train = max(1, int(round(n * ratio[0])))
        n_val = max(1, int(round(n * ratio[1]))) if n >= 10 else max(0, int(round(n * ratio[1])))
        if n_train + n_val >= n:
            n_train = max(1, n - 1)
            n_val = 0
        for i, it in enumerate(order):
            split = "train" if i < n_train else ("val" if i < n_train + n_val else "test")
            plan[(it[1], it[0].name)] = split
    return plan


def download_modelscope(dataset: str, cache: Path) -> Path:
    try:
        from modelscope import snapshot_download  # type: ignore
    except ImportError:
        raise SystemExit(
            "需要 modelscope：\n  pip install modelscope\n"
            "或先手動下載，再用 --src 指向資料夾。"
        )
    eprint(f"[download] {dataset} → {cache}")
    return Path(snapshot_download(dataset, local_dir=str(cache)))


def main() -> int:
    ap = argparse.ArgumentParser(description="garbage265 → 四類 ImageFolder")
    ap.add_argument("--src", type=Path, help="已下載的資料集根目錄")
    ap.add_argument("--download", action="store_true", help="由 ModelScope 下載")
    ap.add_argument("--dataset", default="tany0699/garbage265")
    ap.add_argument("--cache", type=Path, default=Path("~/datasets/garbage265"))
    ap.add_argument("--out", type=Path, required=True)
    ap.add_argument("--mapping", type=Path, help="class→target 的 JSON")
    ap.add_argument("--per-class-limit", type=int, default=0,
                    help="每個目標類別最多取幾張（0 = 不限）")
    ap.add_argument("--min-images", type=int, default=20,
                    help="來源類別少於此數則跳過（預設 20）")
    ap.add_argument("--ratio", default="0.70,0.15,0.15")
    ap.add_argument("--seed", type=int, default=2026)
    ap.add_argument("--no-dedupe", action="store_true")
    ap.add_argument("--copy", choices=["copy", "hardlink", "symlink"], default="copy")
    ap.add_argument("--ei-zip", type=Path, help="輸出 Edge Impulse 用的 zip")
    args = ap.parse_args()

    ratio = tuple(float(x) for x in args.ratio.split(","))
    if len(ratio) != 3 or abs(sum(ratio) - 1.0) > 1e-6:
        raise SystemExit("--ratio 必須是三個加起來等於 1 的數字，例如 0.7,0.15,0.15")

    src = args.src
    if args.download:
        src = download_modelscope(args.dataset, args.cache.expanduser())
    if not src or not Path(src).exists():
        raise SystemExit("找唔到資料集目錄，請用 --src 或 --download")

    src = Path(src)
    layout, classes = detect_layout(src)
    if not classes:
        raise SystemExit(f"在 {src} 找不到任何類別目錄")

    explicit = load_mapping(args.mapping)

    # ---- 映射來源類別 → 目標類別 ----
    mapping_rows = []
    unmapped = []
    for name, imgs in sorted(classes.items()):
        if name in TARGETS:                      # 資料集本身已是四大類
            target = name
        elif name in explicit:
            target = explicit[name]
        else:
            target = keyword_target(name)
        if len(imgs) < args.min_images:
            mapping_rows.append((name, target or "—", len(imgs), "skipped_small"))
            continue
        if not target:
            unmapped.append((name, len(imgs)))
            mapping_rows.append((name, "UNMAPPED", len(imgs), "unmapped"))
            continue
        mapping_rows.append((name, target, len(imgs), "ok"))

    eprint(f"[layout] {layout}；來源類別 {len(classes)} 個；未映射 {len(unmapped)} 個")

    # ---- 收集、去重 ----
    items = []  # (path, source_class, target)
    seen = {}
    dup = 0
    for name, imgs in sorted(classes.items()):
        row = next((r for r in mapping_rows if r[0] == name), None)
        if not row or row[3] != "ok":
            continue
        target = row[1]
        for p in imgs:
            if not args.no_dedupe:
                try:
                    h = file_md5(p)
                except OSError:
                    continue
                if h in seen:
                    dup += 1
                    continue
                seen[h] = p
            items.append((p, name, target))

    eprint(f"[dedupe] 移除重複 {dup} 張；有效 {len(items)} 張")

    # ---- 每個目標類別限額（保持均衡）----
    if args.per_class_limit:
        by_target = defaultdict(list)
        for it in items:
            by_target[it[2]].append(it)
        capped = []
        for t, lst in by_target.items():
            random.Random(args.seed).shuffle(lst)
            capped.extend(lst[: args.per_class_limit])
        items = capped
        eprint(f"[limit] 每類上限 {args.per_class_limit}；保留 {len(items)} 張")

    # ---- 分 split 並落地 ----
    if args.out.exists():
        eprint(f"[warn] {args.out} 已存在，會覆寫同名檔案（不會清空）")
    for split in ("train", "val", "test"):
        for t in TARGETS:
            (args.out / split / t).mkdir(parents=True, exist_ok=True)

    plan = stratified_plan(items, args.seed, ratio)
    labels_path = args.out / "labels.csv"
    counts = Counter()
    with labels_path.open("w", newline="", encoding="utf-8") as f:
        w = csv.writer(f)
        w.writerow(["path", "source_class", "target_class", "split", "src"])
        for p, src_name, target in items:
            split = plan[(src_name, p.name)]
            dst = args.out / split / target / f"{src_name}__{p.name}"
            if dst.exists():
                dst = args.out / split / target / f"{src_name}__{abs(hash(p)) % 10**8}_{p.name}"
            try:
                if args.copy == "hardlink":
                    os.link(p, dst)
                elif args.copy == "symlink":
                    dst.symlink_to(p.resolve())
                else:
                    shutil.copy2(p, dst)
            except OSError as e:
                eprint(f"[skip] {p}: {e}")
                continue
            w.writerow([dst.relative_to(args.out), src_name, target, split, p])
            counts[(split, target)] += 1

    # ---- mapping.csv ----
    with (args.out / "mapping.csv").open("w", newline="", encoding="utf-8") as f:
        w = csv.writer(f)
        w.writerow(["source_class", "target_class", "images", "status"])
        for row in mapping_rows:
            w.writerow(row)

    # ---- report.md ----
    total = sum(counts.values())
    lines = ["# 資料集建置報告", ""]
    lines.append(f"- 來源：`{src}`（layout: `{layout}`）")
    lines.append(f"- 輸出：`{args.out}`")
    lines.append(f"- 總計：**{total}** 張；重複移除 {dup} 張")
    lines.append("")
    lines.append("## 每個 split 的類別分佈")
    lines.append("")
    lines.append("| split | " + " | ".join(TARGETS) + " | 合計 |")
    lines.append("|---|" + "---|" * (len(TARGETS) + 1))
    for split in ("train", "val", "test"):
        row = [str(counts[(split, t)]) for t in TARGETS]
        lines.append(f"| {split} | " + " | ".join(row) + f" | {sum(counts[(split, t)] for t in TARGETS)} |")
    lines.append("")
    if unmapped:
        lines.append("## 未映射的來源類別（需要人手補 mapping.json）")
        lines.append("")
        lines.append("| 來源類別 | 張數 |")
        lines.append("|---|---|")
        for name, n in sorted(unmapped, key=lambda x: -x[1]):
            lines.append(f"| {name} | {n} |")
        lines.append("")
    small = [r for r in mapping_rows if r[3] == "skipped_small"]
    if small:
        lines.append("## 因張數不足而跳過")
        lines.append("")
        for name, _t, n, _s in small:
            lines.append(f"- {name}（{n} 張）")
        lines.append("")
    (args.out / "report.md").write_text("\n".join(lines), encoding="utf-8")

    # ---- Edge Impulse zip（每個類別一個資料夾，不分 split）----
    if args.ei_zip:
        ei_dir = args.out / "_ei_flat"
        if ei_dir.exists():
            shutil.rmtree(ei_dir)
        for t in TARGETS:
            (ei_dir / t).mkdir(parents=True, exist_ok=True)
        for split in ("train", "val", "test"):
            for t in TARGETS:
                for p in (args.out / split / t).glob("*"):
                    shutil.copy2(p, ei_dir / t / p.name)
        with zipfile.ZipFile(args.ei_zip, "w", zipfile.ZIP_DEFLATED) as z:
            for p in ei_dir.rglob("*"):
                if p.is_file():
                    z.write(p, p.relative_to(ei_dir))
        eprint(f"[ei] 已寫出 {args.ei_zip}")

    print(json.dumps({
        "total": total,
        "duplicates_removed": dup,
        "unmapped_classes": len(unmapped),
        "splits": {s: {t: counts[(s, t)] for t in TARGETS} for s in ("train", "val", "test")},
        "out": str(args.out),
    }, ensure_ascii=False, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
