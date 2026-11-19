#!/usr/bin/env python3
"""Apply the stage-one game screens to the Figma-exported layout JSON."""

import copy
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
LAYOUT = ROOT / "design/layout.json"


def text(name, x, y, w, h, value, *, font=32, color="fg"):
    return {
        "name": name,
        "x": x,
        "y": y,
        "w": w,
        "h": h,
        "kind": "text",
        "text": value,
        "font": font,
        "numeric": False,
        "color": color,
    }


def button(name, x, y, w, h, value):
    return {
        "name": name,
        "x": x,
        "y": y,
        "w": w,
        "h": h,
        "kind": "button",
        "text": value,
        "color": "on_brand",
        "background": "brand",
    }


def topbar(status):
    return {
        "name": "comp/topbar",
        "x": 0,
        "y": 0,
        "w": 800,
        "h": 72,
        "kind": "box",
        "background": "brand",
        "radius": 0,
        "children": [],
    }, text("brand", 24, 12, 352, 48, "分類小隊", color="on_brand"), text(
        "status", 424, 12, 352, 48, status, color="on_brand"
    )


def debug_badge():
    return text("debug-badge", 24, 370, 752, 28,
                "REPLAY · VIRTUAL HAND", font=22, color="fg_muted")


def idle_screen(old):
    replacement = copy.deepcopy(old)
    replacement["children"] = [
        node for node in replacement["children"]
        if node.get("name") not in {"action/start", "action/guide", "debug-badge"}
    ]
    replacement["children"].extend([
        button("action/start", 24, 400, 368, 64, "開始挑戰"),
        {
            **button("action/guide", 408, 400, 368, 64, "分類圖鑑"),
            "color": "brand",
            "background": "surface",
        },
        debug_badge(),
    ])
    return replacement


def category_guide_screen(old):
    replacement = copy.deepcopy(old)
    replacement["children"] = [
        node for node in replacement["children"]
        if node.get("name") not in
        {"action/ask", "action/finish", "action/home", "debug-badge"}
    ]
    ask = button("action/ask", 24, 400, 240, 64, "問綠豆")
    ask["color"] = "on_accent"
    ask["background"] = "accent"
    finish = button("action/finish", 280, 400, 240, 64, "返回成績")
    finish["color"] = "brand"
    finish["background"] = "surface"
    home = button("action/home", 536, 400, 240, 64, "返回主頁")
    replacement["children"].extend([ask, finish, home, debug_badge()])
    return replacement


def chat_screen(screens):
    idle = next(s for s in screens if s["name"] == "state/idle")
    bar, brand, status = topbar("CHAT · 綠豆")
    return {
        "name": "state/chat",
        "x": idle["x"],
        "y": idle["y"] + 1600,
        "w": 800,
        "h": 480,
        "kind": "box",
        "background": "bg",
        "radius": 0,
        "children": [
            bar,
            brand,
            status,
            text("prompt", 24, 88, 752, 80, "有咩想問？", font=56),
            text("hint", 24, 176, 752, 40,
                 "CHAT 模式 · 遊戲已暫停", color="ok"),
            {
                "name": "comp/chat-card",
                "x": 24,
                "y": 232,
                "w": 752,
                "h": 120,
                "kind": "box",
                "background": "surface",
                "radius": 16,
                "children": [
                    text("chat-title", 24, 20, 704, 40, "綠豆語音通道"),
                    text("chat-detail", 24, 68, 704, 32,
                         "後端尚未接駁；今次先驗證 GAME／CHAT 外層切換。",
                         font=26, color="fg_muted"),
                ],
            },
            button("action/home", 24, 400, 752, 64, "返回遊戲"),
            debug_badge(),
        ],
        "nodeId": "shell:chat",
    }


def replace_screen(layout, name, screen):
    layout["screens"] = [s for s in layout["screens"] if s["name"] != name]
    layout["screens"].append(screen)


def scan_ready_screen(screens):
    idle = next(s for s in screens if s["name"] == "state/idle")
    bar, brand, status = topbar("回合準備")
    content = {
        "name": "comp/content",
        "x": 24,
        "y": 96,
        "w": 752,
        "h": 280,
        "kind": "box",
        "background": "bg",
        "radius": 0,
        "children": [
            text("prompt", 0, 0, 752, 72, "第 1 / 5 回合", font=56),
            text("hint", 0, 88, 480, 48, "放好卡牌後按掃描"),
            {
                "name": "comp/camera",
                "x": 0,
                "y": 144,
                "w": 480,
                "h": 184,
                "kind": "camera",
            },
            {
                "name": "comp/score-panel",
                "x": 504,
                "y": 88,
                "w": 248,
                "h": 192,
                "kind": "box",
                "background": "surface",
                "radius": 16,
                "children": [],
            },
            text("score-label", 528, 108, 200, 48, "目前分數"),
            text("score", 528, 160, 200, 96, "0", font=80),
        ],
    }
    return {
        "name": "state/scan_ready",
        "x": idle["x"],
        "y": idle["y"] + 1200,
        "w": 800,
        "h": 480,
        "kind": "box",
        "background": "bg",
        "radius": 0,
        "children": [
            bar,
            brand,
            status,
            content,
            button("action/scan", 24, 400, 752, 64, "掃描卡牌"),
        ],
        "nodeId": "stage1:scan-ready",
    }


def bin_choice_screen(screens):
    idle = next(s for s in screens if s["name"] == "state/idle")
    bar, brand, status = topbar("揀投入嘅桶")
    buttons = []
    positions = ((368, 144), (576, 144), (368, 248), (576, 248))
    labels = ("可回收物", "廚餘", "有害", "其他")
    for index, ((x, y), label) in enumerate(zip(positions, labels)):
        item = button(f"action/bin-{index}", x, y, 200, 92, label)
        item["background"] = f"cat{index}"
        buttons.append(item)
    return {
        "name": "state/bin_choice",
        "x": idle["x"],
        "y": idle["y"] + 1400,
        "w": 800,
        "h": 480,
        "kind": "box",
        "background": "bg",
        "radius": 0,
        "children": [
            bar,
            brand,
            status,
            text("prompt", 24, 84, 752, 48, "你將卡放咗入邊個桶？"),
            {
                "name": "comp/camera",
                "x": 24,
                "y": 144,
                "w": 320,
                "h": 184,
                "kind": "camera",
            },
            *buttons,
            text("hint", 24, 336, 752, 28,
                 "答案會喺下一頁揭曉", font=22, color="fg_muted"),
            debug_badge(),
        ],
        "nodeId": "stage1:bin-choice",
    }


def learning_screen(old):
    replacement = copy.deepcopy(old)
    replacement["children"] = [
        topbar("回合回饋")[0],
        topbar("回合回饋")[1],
        topbar("回合回饋")[2],
        text("category", 24, 80, 624, 72, "答對喇", font=56),
        {
            "name": "cat/0",
            "x": 656,
            "y": 72,
            "w": 96,
            "h": 96,
            "kind": "icon",
            "asset": "cat/0",
        },
        text("result", 24, 152, 752, 40, "可回收物放入可回收物桶"),
        {
            "name": "comp/lesson",
            "x": 24,
            "y": 196,
            "w": 752,
            "h": 172,
            "kind": "box",
            "background": "surface",
            "radius": 16,
            "children": [
                text("scanned", 24, 2, 704, 28, "掃到：可回收物", font=22),
                text("dropped", 24, 30, 704, 28, "投入：可回收物", font=22),
                text("answer", 24, 58, 704, 28, "正確：可回收物", font=22),
                text("score", 24, 86, 704, 28, "本回合 +10 · 累計 10", font=22),
                text("tip", 24, 112, 704, 58, "先清空，再沖洗；乾淨回收再造。",
                     font=22, color="fg_muted"),
            ],
        },
        button("action/next", 24, 400, 752, 64, "下一回合"),
        debug_badge(),
    ]
    replacement["nodeId"] = "stage1:learning"
    return replacement


def main():
    layout = json.loads(LAYOUT.read_text())
    scan_ready = scan_ready_screen(layout["screens"])
    learning = learning_screen(next(
        s for s in layout["screens"] if s["name"] == "state/learning"
    ))
    bin_choice = bin_choice_screen(layout["screens"])
    idle = idle_screen(next(s for s in layout["screens"]
                            if s["name"] == "state/idle"))
    guide = category_guide_screen(next(
        s for s in layout["screens"] if s["name"] == "state/category_guide"
    ))
    chat = chat_screen(layout["screens"])
    replace_screen(layout, "state/idle", idle)
    replace_screen(layout, "state/category_guide", guide)
    replace_screen(layout, "state/chat", chat)
    replace_screen(layout, "state/scan_ready", scan_ready)
    replace_screen(layout, "state/bin_choice", bin_choice)
    replace_screen(layout, "state/learning", learning)

    for screen in layout["screens"]:
        screen["children"] = [
            node for node in screen.get("children", [])
            if node.get("name") != "debug-badge"
        ]
        screen["children"].append(debug_badge())

    order = [
        "state/idle", "state/capturing", "state/classifying",
        "state/result_ok", "state/result_retry", "state/error",
        "state/category_guide", "state/learning", "state/summary",
        "state/scan_ready", "state/bin_choice", "state/chat",
    ]
    by_name = {screen["name"]: screen for screen in layout["screens"]}
    layout["screens"] = [by_name[name] for name in order]
    LAYOUT.write_text(json.dumps(layout, ensure_ascii=False, indent=2) + "\n")


if __name__ == "__main__":
    main()
