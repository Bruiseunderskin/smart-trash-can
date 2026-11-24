# Camera confirmation and 綠豆 logout

Status: visual proposal for confirmation. Production UI is unchanged.

## Scope

Only the camera confirmation screen changes.

- Keep the existing LVGL Pro colours, fonts, header, camera frame and countdown.
- Move the title and supporting copy together as one vertically centred group.
- Place the 綠豆 icon immediately to the left of the title "確認卡片完整入框".
- The 綠豆 icon is the hidden logout control.

## Layout

```text
┌──────────────────────────────────────────────────────────────┐
│ 分類小隊                              第 3 / 5 回合           │
├──────────────────────────────────────────────────────────────┤
│ ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━                │
│                                                              │
│  ┌────────────────────┐       ┌───┐                         │
│  │                    │       │綠豆│  確認卡片完整入框      │
│  │    相機畫面        │       └───┘                         │
│  │                    │                                      │
│  └────────────────────┘            5 秒後自動掃描           │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

The camera remains at `x=24, y=112, 320×320`.

The right-side content uses the same `320px` vertical band as the camera:

- Group aligned vertically in the centre of the camera.
- Title row: 64px 綠豆 icon + 16px gap + two-line "確認卡片／完整入框".
- Supporting copy: "5 秒後自動掃描", centred below with 40px separation.

## Hidden logout

- Long-press 綠豆 for 1.2 seconds.
- Show a progress ring while held.
- A normal short tap does not leave the screen.
- Confirmation text: "返回主頁？返回主頁會取消掃描，分數不動。桶蓋開，返回前會先關閉。"
- Actions: "繼續遊戲" and "返回主頁".
- Confirm cancels scan/inference, stops retry audio, closes lids if open,
  discards the round result and returns home.

## Implementation subtasks

1. Add a 64x64 long-press target beside "確認入框".
2. Shift the complete title/detail group to vertical centre.
3. Add the 1.2s hold progress feedback.
4. Add confirm/cancel modal.
5. Add controller cancellation for scan/inference and stale results.
6. Stop retry audio and close lids safely on confirmed logout.
7. Render 800x480 screenshots and run native interaction tests.
8. Re-run S31/S3 hardware smoke.
