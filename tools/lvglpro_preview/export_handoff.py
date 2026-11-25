#!/usr/bin/env python3
"""Export an AI-ready handoff from the live ui/lvglpro renderer.

The screenshots are the exact LVGL framebuffer at 800x480. The layout JSON is
read from the same running LVGL object tree, so coordinates are not guessed
from the PNG.
"""
from __future__ import annotations

import argparse
import base64
import csv
import hashlib
import html
import io
import json
import os
import subprocess
import sys
from pathlib import Path
from typing import Any

from PIL import Image, ImageChops

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
sys.path.insert(0, str(HERE))
from render import DIFF_LIMIT, H, PREVIEW, STATES, W  # noqa: E402

DEFAULT_OUT = ROOT / "ui" / "handoff"
TMP = HERE / "build" / "handoff-export"
LVGL_VERSION = "v9.5.0"
LVGL_COMMIT = "85aa60d18b3d5e5588d7b247abf90198f07c8a63"

SCREEN_META: dict[str, dict[str, Any]] = {
    "idle": {
        "title": "主頁",
        "group": "game",
        "description": "開始遊戲或進入綠豆對話；主頁沒有長按操作。",
        "phase": "idle",
    },
    "intro": {
        "title": "玩法說明",
        "group": "game",
        "description": "三步玩法：放在鏡頭前、投入垃圾桶、等蓋關閉。",
        "phase": "onboarding",
        "optional": True,
    },
    "scan_ready": {
        "title": "確認卡片完整入框",
        "group": "game",
        "description": "顯示相機預覽；5 秒倒數後自動掃描。綠豆長按可退出回合。",
        "phase": "prompt",
    },
    "scan_logout_confirm": {
        "title": "退出回合確認",
        "group": "game",
        "description": "scan_ready 長按綠豆後顯示的確認覆蓋層。",
        "phase": "prompt_overlay",
        "variantOf": "scan_ready",
    },
    "capturing": {
        "title": "正在辨識",
        "group": "game",
        "description": "相機畫面與掃描動畫；玩家在此階段不需要操作。",
        "phase": "scan",
    },
    "classifying": {
        "title": "整理緊結果",
        "group": "game",
        "description": "影像已取得，等待模型整理結果。",
        "phase": "scan_settle",
    },
    "result_ok": {
        "title": "正在開啟桶蓋",
        "group": "game",
        "description": "S3 開啟四個桶蓋；模型類別保持隱藏，避免在投放前洩露答案。",
        "phase": "result_open",
    },
    "result_ok_2s": {
        "title": "開啟桶蓋（2 秒）",
        "group": "game",
        "description": "result_ok 的 2 秒時間點，用於示範頂部時間條收縮。",
        "phase": "result_open",
        "variantOf": "result_ok",
    },
    "result_ok_lid": {
        "title": "桶蓋已開啟",
        "group": "game",
        "description": "result_ok 的 lid_open 變體；實際硬件回報蓋已開啟。",
        "phase": "open_ack",
        "variantOf": "result_ok",
    },
    "result_retry": {
        "title": "AI 都唔肯定",
        "group": "game",
        "description": "顯示兩個候選類別，讓玩家重新掃描或返回主頁。",
        "phase": "retry",
    },
    "bin_choice": {
        "title": "請將卡牌放入垃圾桶",
        "group": "game",
        "description": "四個桶已開啟，等待 S3 TOF 回報實際投放。",
        "phase": "wait_drop",
    },
    "error": {
        "title": "系統提示",
        "group": "game",
        "description": "錯誤代碼對應的玩家文案與重試操作。",
        "phase": "error",
    },
    "learning": {
        "title": "回合學習",
        "group": "game",
        "description": "顯示掃描類別、實際投放、正確類別、得分與分類提示。",
        "phase": "learning",
    },
    "summary": {
        "title": "本局總結",
        "group": "game",
        "description": "總分、完成回合、最高連對，以及再玩一次／返回主頁。",
        "phase": "summary",
    },
    "ask_ready": {
        "title": "問綠豆：準備",
        "group": "chat",
        "description": "綠豆對話待命畫面。",
        "phase": "chat_ready",
    },
    "ask_listening": {
        "title": "問綠豆：聆聽中",
        "group": "chat",
        "description": "咪高峰正在接收玩家語音。",
        "phase": "chat_listening",
    },
    "ask_thinking": {
        "title": "問綠豆：思考中",
        "group": "chat",
        "description": "已收到問題，等待回覆。",
        "phase": "chat_thinking",
    },
    "ask_answering": {
        "title": "問綠豆：回答",
        "group": "chat",
        "description": "顯示綠豆的短回答。",
        "phase": "chat_answering",
    },
    "ask_answering_long": {
        "title": "問綠豆：長回答",
        "group": "chat",
        "description": "長回答的換行與滾動區域變體。",
        "phase": "chat_answering",
        "variantOf": "ask_answering",
    },
    "ask_offline": {
        "title": "問綠豆：暫時離線",
        "group": "chat",
        "description": "語音服務不可用時的離線提示。",
        "phase": "chat_offline",
    },
    "ask_asr_retry": {
        "title": "問綠豆：再講一次",
        "group": "chat",
        "description": "語音辨識失敗後的重試提示。",
        "phase": "chat_asr_retry",
    },
    "ask_countdown": {
        "title": "問綠豆：返回倒數",
        "group": "chat",
        "description": "對話完成後返回遊戲的倒數狀態。",
        "phase": "chat_countdown",
    },
    "ask_no_answer": {
        "title": "問綠豆：未有回應",
        "group": "chat",
        "description": "沒有收到回答時的提示。",
        "phase": "chat_no_answer",
    },
    "ask_goodbye": {
        "title": "問綠豆：再見",
        "group": "chat",
        "description": "結束對話並返回遊戲。",
        "phase": "chat_goodbye",
    },
    "ask_connecting": {
        "title": "問綠豆：連線中",
        "group": "chat",
        "description": "正在連接語音服務。",
        "phase": "chat_connecting",
    },
    "ask_provisioning": {
        "title": "問綠豆：設定網絡",
        "group": "chat",
        "description": "裝置需要完成 Wi-Fi 或語音服務設定。",
        "phase": "chat_provisioning",
    },
}

MOTION_CAPTURES = {
    "scan_ready": [2500, 5000],
    "capturing": [250, 500, 750, 1000],
    "classifying": [250, 500],
    "result_ok": [1000, 2000, 3000],
    "learning": [3000, 6000],
}

BUTTON_ACTIONS = {
    "開始遊戲": "start_game",
    "開始掃描": "scan",
    "重新掃描": "scan",
    "返回主頁": "home",
    "下一回合": "next",
    "再玩一次": "start_game",
    "問綠豆": "ask_greenbean",
    "返回遊戲": "return_to_game",
    "重新連線": "reconnect",
}

TOKENS = {
    "canvas": {
        "width": W,
        "height": H,
        "rotation": 0,
        "colorDepth": 16,
        "pixelFormat": "RGB565",
    },
    "color": {
        "background": "#FBF7EC",
        "surface": "#FFFFFF",
        "foreground": "#14342B",
        "muted": "#5A6B62",
        "error": "#C2532F",
        "brand": "#14342B",
        "onBrand": "#FBF7EC",
        "accent": "#7FD8B0",
        "onAccent": "#14342B",
        "brandLight": "#2C6B4F",
        "celebrate": "#DCE45A",
        "line": "#E2D9C6",
        "category": ["#2455C0", "#176640", "#9E3F20", "#4E5A62"],
    },
    "type": {
        "caption": {"family": "TC medium", "px": 26, "lineHeightSample": 38},
        "body": {"family": "TC medium", "px": 32},
        "bodyBold": {"family": "TC bold", "px": 32},
        "subtitle": {"family": "TC medium", "px": 40},
        "h2": {"family": "TC bold", "px": 40},
        "h1": {"family": "TC bold", "px": 56},
        "category": {"family": "TC bold", "px": 112},
        "numberBody": {"family": "numeric bold", "px": 32},
        "numberH2": {"family": "numeric bold", "px": 40},
        "numberScore": {"family": "numeric bold", "px": 80},
    },
    "space": {
        "outer": 24,
        "cardPadding": 24,
        "compactPadding": 16,
        "iconGap": 24,
        "sectionGap": 16,
        "textGap": 8,
        "controlHeight": 64,
    },
    "anchors": {
        "topBar": {"x": 0, "y": 0, "w": 800, "h": 72},
        "greenBean": {"x": 688, "y": 112, "w": 64, "h": 64},
        "cameraFrame": {"x": 24, "y": 112, "w": 360, "h": 360},
    },
    "motion": {
        "pageTransition": "cut",
        "scanReadyAutoMs": 5000,
        "resultAutoMs": 3000,
        "scanOpacityAnimation": True,
        "touchPressedOpacity": 210,
    },
}

TIMINGS = {
    "promptTimeoutMs": 20000,
    "scanReadyAutoMs": 5000,
    "scanTimeoutMs": 1500,
    "resultMs": 1500,
    "openTimeoutMs": 1500,
    "waitDropMs": 15000,
    "settleMs": 300,
    "closeTimeoutMs": 1500,
    "learningMs": 6000,
    "gameTotalMs": 180000,
    "rounds": 5,
}


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def write_json(path: Path, value: Any) -> None:
    path.write_text(
        json.dumps(value, ensure_ascii=False, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )


def walk(node: dict[str, Any]):
    yield node
    for child in node.get("children", []):
        yield from walk(child)


def descendants(node: dict[str, Any]):
    for child in node.get("children", []):
        yield child
        yield from descendants(child)


def label_text(node: dict[str, Any]) -> str:
    parts = [n["text"] for n in descendants(node) if n.get("widget") == "label"]
    return "".join(parts)


def derived_layout(layout: dict[str, Any]) -> dict[str, Any]:
    nodes = list(walk(layout["root"]))
    labels = [
        {
            key: node[key]
            for key in (
                "id",
                "x",
                "y",
                "w",
                "h",
                "text",
                "textColor",
                "fontLineHeight",
                "textAlign",
            )
        }
        for node in nodes
        if node.get("widget") == "label"
    ]
    images = [
        {key: node[key] for key in ("id", "x", "y", "w", "h")}
        for node in nodes
        if node.get("widget") == "image"
    ]
    buttons = []
    for node in nodes:
        if node.get("widget") != "button":
            continue
        text = label_text(node)
        buttons.append(
            {
                "id": node["id"],
                "x": node["x"],
                "y": node["y"],
                "w": node["w"],
                "h": node["h"],
                "label": text,
                "role": "text_button" if text else "greenbean_icon",
                "action": BUTTON_ACTIONS.get(text, "greenbean" if not text else "unknown"),
            }
        )
    progress_bars = [
        {key: node[key] for key in ("id", "x", "y", "w", "h", "bg")}
        for node in nodes
        if node.get("widget") == "object"
        and 0 < node.get("h", 0) <= 12
        and node.get("w", 0) >= 300
    ]
    layout["derived"] = {
        "labels": labels,
        "images": images,
        "buttons": buttons,
        "progressBars": progress_bars,
    }
    return layout


def run_capture(
    state: str,
    ticks: int,
    *,
    layout_path: Path | None = None,
    reduced_motion: bool = True,
) -> Image.Image:
    TMP.mkdir(parents=True, exist_ok=True)
    raw = TMP / f"{state}-{ticks}.raw"
    env = os.environ.copy()
    env["STC_PREVIEW_REDUCED_MOTION"] = "1" if reduced_motion else "0"
    if layout_path is None:
        env.pop("STC_PREVIEW_LAYOUT_PATH", None)
    else:
        layout_path.parent.mkdir(parents=True, exist_ok=True)
        env["STC_PREVIEW_LAYOUT_PATH"] = str(layout_path)
    proc = subprocess.run(
        [str(PREVIEW), state, str(ticks), str(raw)],
        capture_output=True,
        text=True,
        check=False,
        env=env,
    )
    if proc.returncode != 0 or "layout=FAIL" in proc.stdout:
        raise RuntimeError(f"{state} @ {ticks}ms failed:\n{proc.stdout}{proc.stderr}")
    data = raw.read_bytes()
    raw.unlink(missing_ok=True)
    if len(data) != W * H * 2:
        raise RuntimeError(f"{state}: expected {W * H * 2} bytes, got {len(data)}")
    return Image.frombytes("RGB", (W, H), data, "raw", "BGR;16")


def baseline_difference(image: Image.Image, baseline: Path) -> float | None:
    if not baseline.exists():
        return None
    reference = Image.open(baseline).convert("RGB")
    diff = ImageChops.difference(image, reference)
    raw_diff = diff.tobytes()
    changed = sum(
        1
        for offset in range(0, len(raw_diff), 3)
        if raw_diff[offset] or raw_diff[offset + 1] or raw_diff[offset + 2]
    )
    return changed / (W * H)


def embedded_png(image: Image.Image) -> str:
    buffer = io.BytesIO()
    image.save(buffer, format="PNG", optimize=True)
    return base64.b64encode(buffer.getvalue()).decode("ascii")


def make_svg(meta: dict[str, Any], image: Image.Image) -> str:
    return f'''<svg xmlns="http://www.w3.org/2000/svg" width="{W}" height="{H}" viewBox="0 0 {W} {H}">
  <title>{html.escape(meta['title'])} ({html.escape(meta['state'])})</title>
  <desc>{html.escape(meta['description'])}</desc>
  <image width="{W}" height="{H}" href="data:image/png;base64,{embedded_png(image)}"/>
</svg>
'''


def make_atlas(records: list[dict[str, Any]], images: dict[str, Image.Image]) -> str:
    cols = 4
    thumb_w, thumb_h = 360, 216
    gap_x, gap_y, label_h, margin = 28, 22, 34, 28
    rows = (len(records) + cols - 1) // cols
    board_w = margin * 2 + cols * thumb_w + (cols - 1) * gap_x
    board_h = margin * 2 + rows * (thumb_h + label_h) + (rows - 1) * gap_y
    parts = [
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{board_w}" height="{board_h}" viewBox="0 0 {board_w} {board_h}">',
        '<rect width="100%" height="100%" fill="#D9D9D9"/>',
    ]
    for index, record in enumerate(records):
        state = record["state"]
        col, row = index % cols, index // cols
        x = margin + col * (thumb_w + gap_x)
        y = margin + row * (thumb_h + label_h + gap_y)
        thumb = images[state].resize((thumb_w, thumb_h), Image.Resampling.LANCZOS)
        parts.append(
            f'<g id="{html.escape(state)}" transform="translate({x} {y})">'
            f'<rect width="{thumb_w}" height="{thumb_h}" fill="#FFFFFF"/>'
            f'<image width="{thumb_w}" height="{thumb_h}" href="data:image/png;base64,{embedded_png(thumb)}"/>'
            f'<text x="0" y="{thumb_h + 24}" font-family="Arial, sans-serif" font-size="20" fill="#14342B">'
            f'{html.escape(state)}</text></g>'
        )
    parts.append("</svg>\n")
    return "\n".join(parts)


def make_index(manifest: dict[str, Any]) -> str:
    cards = []
    for record in manifest["screens"]:
        cards.append(
            f'''<article>
  <a href="screens/{html.escape(record['state'])}.png"><img src="screens/{html.escape(record['state'])}.png" alt="{html.escape(record['title'])}"></a>
  <h2>{html.escape(record['title'])}</h2>
  <code>{html.escape(record['state'])}</code>
  <p>{html.escape(record['description'])}</p>
  <p><a href="layouts/{html.escape(record['state'])}.json">layout JSON</a> · <a href="figma-import/{html.escape(record['state'])}.svg">SVG</a></p>
</article>'''
        )
    return f'''<!doctype html>
<html lang="zh-Hant">
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>分類小隊 UI handoff</title>
<style>
:root {{ color-scheme: light; --ink:#14342B; --line:#D8D2C4; --paper:#FBF7EC; --card:#fff; }}
body {{ margin:0; padding:32px; font-family:system-ui,-apple-system,"Noto Sans TC",sans-serif; background:var(--paper); color:var(--ink); }}
header {{ max-width:1280px; margin:0 auto 28px; }}
h1 {{ font-size:42px; margin:0 0 12px; }}
p {{ line-height:1.55; }}
.grid {{ max-width:1280px; margin:0 auto; display:grid; grid-template-columns:repeat(auto-fit,minmax(320px,1fr)); gap:20px; }}
article {{ background:var(--card); border:1px solid var(--line); border-radius:18px; padding:16px; box-shadow:0 8px 24px #14342b12; }}
img {{ display:block; width:100%; height:auto; border-radius:10px; background:#eee; }}
h2 {{ font-size:24px; margin:14px 0 4px; }}
code {{ background:#E9F4EE; padding:4px 8px; border-radius:6px; }}
a {{ color:#145C3E; }}
</style>
<header>
<h1>分類小隊 · 800×480 UI handoff</h1>
<p>所有圖片直接由 LVGL 9.5 RGB565 framebuffer 匯出。layout JSON 由同一個 LVGL object tree 讀取。此頁只作預覽；Remotion 請讀 <code>remotion-manifest.json</code>。</p>
</header>
<main class="grid">{''.join(cards)}</main>
</html>
'''


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=Path, default=DEFAULT_OUT)
    parser.add_argument(
        "--skip-motion",
        action="store_true",
        help="Do not generate the extra non-reduced-motion sample frames.",
    )
    args = parser.parse_args()
    out = args.output.resolve()

    if not PREVIEW.exists():
        raise SystemExit(f"{PREVIEW} is missing; build tools/lvglpro_preview first")

    for name in ("screens", "layouts", "motion", "figma-import"):
        (out / name).mkdir(parents=True, exist_ok=True)

    records: list[dict[str, Any]] = []
    images: dict[str, Image.Image] = {}
    hashes: dict[str, str] = {}

    for state, ticks in STATES:
        meta = dict(SCREEN_META[state])
        meta["state"] = state
        layout_path = out / "layouts" / f"{state}.json"
        image = run_capture(
            state,
            ticks,
            layout_path=layout_path,
            reduced_motion=True,
        )
        png_path = out / "screens" / f"{state}.png"
        image.save(png_path, format="PNG", optimize=True)
        images[state] = image
        layout = json.loads(layout_path.read_text(encoding="utf-8"))
        layout = derived_layout(layout)
        write_json(layout_path, layout)
        (out / "figma-import" / f"{state}.svg").write_text(
            make_svg(meta, image), encoding="utf-8"
        )
        baseline_ratio = baseline_difference(
            image, HERE / "baselines" / f"{state}.png"
        )
        if baseline_ratio is not None and baseline_ratio > DIFF_LIMIT:
            raise RuntimeError(
                f"{state}: rendered frame differs from baseline by "
                f"{baseline_ratio:.3%}"
            )
        record = {
            **meta,
            "file": f"screens/{state}.png",
            "layout": f"layouts/{state}.json",
            "svg": f"figma-import/{state}.svg",
            "pngSha256": sha256(png_path),
            "layoutSha256": sha256(layout_path),
            "baselineDiffRatio": baseline_ratio,
            "previewTicksMs": ticks,
            "previewReducedMotion": True,
        }
        if state == "scan_ready":
            record["gestures"] = [
                {
                    "type": "long_press",
                    "rect": TOKENS["anchors"]["greenBean"],
                    "action": "cancel_round",
                }
            ]
        records.append(record)
        hashes[state] = record["pngSha256"]

    motion_records = []
    if not args.skip_motion:
        for state, tick_values in MOTION_CAPTURES.items():
            for ticks in tick_values:
                image = run_capture(state, ticks, reduced_motion=False)
                name = f"{state}_{ticks}ms"
                path = out / "motion" / f"{name}.png"
                image.save(path, format="PNG", optimize=True)
                motion_records.append(
                    {
                        "state": state,
                        "tMs": ticks,
                        "file": f"motion/{name}.png",
                        "pngSha256": sha256(path),
                        "previewReducedMotion": False,
                    }
                )

    (out / "figma-import" / "screen-atlas.svg").write_text(
        make_atlas(records, images), encoding="utf-8"
    )

    flows = [
        {
            "id": "live_round_happy_path",
            "title": "實機一回合：掃描、開蓋、投放、學習",
            "transitionMode": "cut",
            "steps": [
                {"state": "idle", "holdMs": 1800, "event": "tap_start"},
                {"state": "scan_ready", "holdMs": 5000, "event": "auto_scan"},
                {"state": "capturing", "holdMs": 1200, "event": "auto"},
                {"state": "classifying", "holdMs": 300, "event": "auto"},
                {"state": "result_ok", "holdMs": 1500, "event": "auto"},
                {"state": "result_ok_2s", "holdMs": 1500, "event": "lid_open"},
                {"state": "bin_choice", "holdMs": 2500, "event": "tof_drop"},
                {"state": "learning", "holdMs": 6000, "event": "auto"},
                {"state": "summary", "holdMs": 4000, "event": "end"},
            ],
        },
        {
            "id": "five_round_loop",
            "title": "五回合主循環",
            "transitionMode": "cut",
            "roundSequence": [
                "scan_ready",
                "capturing",
                "classifying",
                "result_ok",
                "bin_choice",
                "learning",
            ],
            "rounds": 5,
            "afterEachRound": "scan_ready except after round 5 -> summary",
        },
        {
            "id": "scan_retry",
            "title": "不確定／重新掃描",
            "transitionMode": "cut",
            "steps": [
                {"state": "scan_ready", "holdMs": 5000},
                {"state": "capturing", "holdMs": 1200},
                {"state": "classifying", "holdMs": 300},
                {"state": "result_retry", "holdMs": 2500},
                {"state": "scan_ready", "holdMs": 5000},
            ],
        },
        {
            "id": "greenbean_chat",
            "title": "綠豆對話",
            "transitionMode": "cut",
            "steps": [
                {"state": "idle", "holdMs": 1200, "event": "tap_ask_greenbean"},
                {"state": "ask_ready", "holdMs": 1200},
                {"state": "ask_listening", "holdMs": 2400},
                {"state": "ask_thinking", "holdMs": 1200},
                {"state": "ask_answering", "holdMs": 3200},
                {"state": "ask_goodbye", "holdMs": 1600},
                {"state": "idle", "holdMs": 1200},
            ],
        },
        {
            "id": "round_logout",
            "title": "長按綠豆退出回合",
            "transitionMode": "cut",
            "steps": [
                {"state": "scan_ready", "holdMs": 1500, "event": "long_press_greenbean"},
                {"state": "scan_logout_confirm", "holdMs": 1300},
                {"state": "idle", "holdMs": 1200},
            ],
        },
        {
            "id": "onboarding",
            "title": "首次玩法說明",
            "transitionMode": "cut",
            "optional": True,
            "steps": [
                {"state": "idle", "holdMs": 1400, "event": "tap_start"},
                {"state": "intro", "holdMs": 2400},
                {"state": "scan_ready", "holdMs": 5000},
            ],
        },
    ]

    source_files = [
        ROOT / "ui" / "lvglpro" / "src" / "stc_ui.c",
        ROOT / "ui" / "lvglpro" / "src" / "stc_chat_ui.c",
        ROOT / "ui" / "lvglpro" / "include" / "stc_ui.h",
    ]
    manifest = {
        "schemaVersion": "1.0",
        "kind": "lvgl-pixel-and-object-tree-handoff",
        "canvas": TOKENS["canvas"],
        "visualSource": {
            "renderer": f"LVGL {LVGL_VERSION}",
            "lvglCommit": LVGL_COMMIT,
            "preview": "tools/lvglpro_preview/build/preview",
            "uiSource": "ui/lvglpro/src/stc_ui.c",
            "chatSource": "ui/lvglpro/src/stc_chat_ui.c",
            "sourceFiles": [
                {"path": str(path.relative_to(ROOT)), "sha256": sha256(path)}
                for path in source_files
            ],
        },
        "tokens": TOKENS,
        "timingsMs": TIMINGS,
        "screenCount": len(records),
        "screens": records,
        "motionFrames": motion_records,
        "flows": flows,
        "notes": [
            "PNG files are exact 800x480 LVGL framebuffer renders expanded from RGB565.",
            "Layout JSON coordinates are absolute pixels captured from the running LVGL object tree.",
            "Baseline screenshots use reduced motion. Extra frames under motion/ use normal motion at the stated tick.",
            "LVGL has no page slide/fade transition; preserve the real cut between screens.",
            "A physical LCD can still differ in panel colour, gamma and viewing angle; this package matches the LVGL framebuffer.",
        ],
    }
    write_json(out / "manifest.json", manifest)
    write_json(out / "tokens.json", TOKENS)
    write_json(
        out / "remotion-manifest.json",
        {
            "schemaVersion": "1.0",
            "composition": {
                "id": "SmartTrashCanUI",
                "width": W,
                "height": H,
                "fps": 60,
                "durationInFrames": 1800,
            },
            "assets": {
                "screens": "screens/{state}.png",
                "layouts": "layouts/{state}.json",
                "motion": "motion/{state}_{tickMs}ms.png",
                "figmaSvgs": "figma-import/{state}.svg",
            },
            "renderGuidance": {
                "fit": "contain",
                "background": "#FBF7EC",
                "screenTransition": "cut",
                "pointer": "simulate touch only; hardware input is not a mouse cursor",
                "greenbeanAnchorPx": TOKENS["anchors"]["greenBean"],
            },
            "timingsMs": TIMINGS,
            "flows": flows,
        },
    )
    write_json(
        out / "verification.json",
        {
            "canvas": {"width": W, "height": H},
            "screenCount": len(records),
            "screens": {
                record["state"]: {
                    "pngSha256": record["pngSha256"],
                    "layoutSha256": record["layoutSha256"],
                    "baselineDiffRatio": record["baselineDiffRatio"],
                }
                for record in records
            },
            "motionFrameCount": len(motion_records),
        },
    )

    with (out / "screen-index.csv").open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(
            handle,
            fieldnames=[
                "state",
                "group",
                "title",
                "phase",
                "png",
                "layout",
                "svg",
                "description",
            ],
        )
        writer.writeheader()
        for record in records:
            writer.writerow(
                {
                    "state": record["state"],
                    "group": record["group"],
                    "title": record["title"],
                    "phase": record["phase"],
                    "png": record["file"],
                    "layout": record["layout"],
                    "svg": record["svg"],
                    "description": record["description"],
                }
            )

    (out / "index.html").write_text(make_index(manifest), encoding="utf-8")
    (out / "flow.mmd").write_text(
        "flowchart LR\n"
        "  idle[主頁] --> scan_ready[確認卡片入框]\n"
        "  scan_ready -->|5 秒自動| capturing[正在辨識]\n"
        "  capturing --> classifying[整理緊結果]\n"
        "  classifying --> result_ok[正在開啟桶蓋]\n"
        "  result_ok --> bin_choice[請將卡牌放入垃圾桶]\n"
        "  bin_choice -->|TOF DROP| learning[回合學習]\n"
        "  learning -->|下一回合| scan_ready\n"
        "  learning -->|第 5 回合| summary[本局總結]\n"
        "  idle --> ask_ready[問綠豆]\n"
        "  ask_ready --> ask_listening --> ask_thinking --> ask_answering --> ask_goodbye\n"
        "  ask_goodbye --> idle\n"
        "  scan_ready -->|長按綠豆| scan_logout_confirm[退出確認]\n"
        "  scan_logout_confirm --> idle\n",
        encoding="utf-8",
    )

    readmes = {
        out / "README.md": f'''# 800×480 UI handoff

讀者：設計師、Remotion／動畫 AI、要重建畫面的工程人員。何時讀：需要實機 UI 的像素、文字、座標或操作流程時。

- `screens/`：{len(records)} 張由 LVGL 9.5 framebuffer 直接輸出的 800×480 PNG。
- `layouts/`：同一批畫面在 LVGL 執行時的 object tree，座標是 absolute pixels，不是估算。
- `motion/`：指定時間點的正常動畫 frame；baseline PNG 本身使用 reduced motion。
- `figma-import/`：每張圖一個可直接拖入 Figma 的 SVG，另有 `screen-atlas.svg`。
- `manifest.json`：完整 machine-readable source of truth，包括 tokens、screen metadata、hash、flow。
- `remotion-manifest.json`：Remotion composition、assets、timings 與操作流程建議。
- `remotion-prompt.md`：可直接貼給 Remotion AI 的完整限制與工作指示。
- `index.html`：本地視覺索引；直接開檔即可。
- `flow.mmd`：Mermaid 流程圖。
- `screen-index.csv`：表格化清單，方便其他工具快速索引。

## 精確度

PNG 是 800×480、rotation 0、RGB565 framebuffer 展開後輸出；layout JSON 由同一個 running LVGL object tree 讀取。頁面在真機是 instant cut，沒有全局 slide／fade。實體 LCD 仍可能因 panel gamma、色域及視角與 framebuffer 有差異。

## Remotion 建議

1. 以 `remotion-manifest.json` 的 `flows` 建立 scene sequence。
2. 每個 state 用 `screens/{{state}}.png` 作 full-frame asset，保持 800×480、contain、不要重新裁切。
3. 用 `layouts/{{state}}.json` 的 button bounds 放觸控波紋；不要顯示 desktop mouse cursor。
4. 倒數與進度只可動 `derived.progressBars` 對應的矩形；其他版面保持不動。
5. 轉頁用 hard cut；需要動畫時用 `motion/` frame 或局部 opacity 動畫，不要整頁 fade。
6. 綠豆在所有遊戲頁固定在 `(688,112)`、64×64；不要為個別 scene 重排。

## 重建

```sh
tools/lvglpro_preview/build.sh
python tools/lvglpro_preview/render.py --compare tools/lvglpro_preview/baselines
python tools/lvglpro_preview/export_handoff.py
```

最後一步會重新產生整個資料夾。只可在已核對 baseline 的 UI source 上執行。
''',
        out / "screens" / "README.md": '''# Screens

800×480 PNG，直接由 `ui/lvglpro` 的 LVGL framebuffer 輸出。檔名就是 state id。不要在 Remotion 內重新壓縮或 resize 到非 16:9 比例；輸出時直接用 800×480。

[Handoff 首頁](../README.md)
''',
        out / "layouts" / "README.md": '''# Layouts

每張畫面一個 JSON。`root` 是 LVGL object tree；`x`、`y`、`w`、`h` 是全畫面 absolute pixels。`derived` 提供扁平化的 labels、images、buttons 及 progressBars，方便 Remotion AI 直接使用。

[Handoff 首頁](../README.md)
''',
        out / "motion" / "README.md": '''# Motion frames

指定 tick 的正常動畫 frame，檔名格式 `<state>_<ms>ms.png`。這批 frame 關閉 reduced motion；主要 baseline screenshots 則開啟 reduced motion。請只參考時間條、掃描效果等局部變化，不要改動其餘版面。

[Handoff 首頁](../README.md)
''',
        out / "figma-import" / "README.md": '''# Figma import

每張 SVG 是 800×480 pixel-exact framebuffer image，可直接拖入 Figma。`screen-atlas.svg` 將全部畫面排成一張 board。這些 SVG 是視覺參考，不是重新建立的 editable component tree；可編輯結構請用 `layouts/`。

[Handoff 首頁](../README.md)
''',
        out / "remotion-prompt.md": '''# 可直接交給 Remotion AI 的工作指示

你要用 Remotion 製作「分類小隊」S31 實機 UI 的模擬操作流程動畫。唯一視覺 source of truth 是本資料夾，不要自行重畫、改字、改色或移動元件。

## 必讀

1. 先讀 `remotion-manifest.json`、`manifest.json` 及相關 `layouts/<state>.json`。
2. 畫面固定 800×480、rotation 0、60 fps；用 `screens/<state>.png` 作 full-frame asset。
3. 以 `flows` 建立 sequence。主要流程用 `live_round_happy_path`；五回合用 `five_round_loop`。
4. 轉頁保持 hard cut。LVGL 真機沒有全頁 fade／slide。
5. 觸控只畫局部 ripple，位置用 layout 的 `derived.buttons`；不要顯示 desktop mouse cursor。
6. 進度條只可按 `derived.progressBars` 的 bounds 動畫，不可移動其他元素。
7. 綠豆固定在 `(688,112)`、64×64；只有 `scan_ready` 有長按退出。
8. `motion/` frame 是正常動畫取樣，只可用在掃描效果與時間條；其餘畫面保持 baseline。
9. 不要加入真實硬件畫面、3D 垃圾桶或 UI 內不存在的按鈕。
10. 輸出時保留 800×480 composition；如需 16:9 展示可整張等比放大，不可重新排版。

## 建議元件

```tsx
const shot = (state: string) => staticFile(`screens/${state}.png`);
<Img src={shot(state)} style={{ width: 800, height: 480 }} />
```

每個 scene 可先用 hard cut 顯示 PNG，再用 `interpolate` 局部遮蓋 progress bar；需要掃描動態時，切換 `motion/capturing_250ms.png` 至 `motion/capturing_1000ms.png`。完成後請自行核對：所有畫面尺寸、文字、四桶位置及綠豆 anchor 與 handoff 一致。
''',
    }
    for path, text in readmes.items():
        path.write_text(text, encoding="utf-8")

    print(
        f"exported {len(records)} screens, {len(motion_records)} motion frames -> {out}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())