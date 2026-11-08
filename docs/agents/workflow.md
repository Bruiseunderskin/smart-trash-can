# 工作流程：六道閘

這份文件存在的理由很具體：同一個改動，曾經出現過四個錯——改錯對象（在 PR #41 而不是 issue #45）、數字前後不一（60 s 與 90 s 並存）、PR 描述與 diff 不符、以及把已撤回的改動留在另一條分支。

四個錯有同一個根源：**動作快過核對**。以下六道閘是按「哪一步可以攔住哪一種錯」排的，不是按流程美觀排。

---

## 閘 1 — 認清對象（攔「改錯東西」）

Operator 講一個編號時，先確認它究竟是 issue 還是 PR。

```bash
gh issue view <N> --json number,title,state
gh pr view    <N> --json number,title,state,headRefName
```

GitHub 的 issue 與 PR 共用編號，`gh issue view 41` 會把 PR 41 當 issue 回傳。所以要看回來的 `url` 與 `title`：

- 是 PR → 再找它對應的 issue（body 內的 `Closes #N`）
- 是 issue → 由 issue 開新分支，不要寄生在別人的 PR 分支上

**判準**：如果改動屬於 issue 的驗收範圍，就在 main 開新分支、PR 寫 `Closes #N`。寄生在無關 PR 的分支上，等於令那條 PR 的範圍失控。

## 閘 2 — 一句話回述（攔「理解錯」）

動手前，用一行講清楚：

> 改 `docs/mode-gate.md` 的轉移表 ＋ `contracts/stc_mode.h` 的 enum；不改 timeout；升版因為新增輸入。

對方一句 OK 或者直接更正，成本是一行字；錯了要重做的是幾十分鐘。

## 閘 3 — 單一數字來源（攔「前後不一」）

任何常數只可以在**一個地方**有權威值：

| 常數 | 權威位置 | 其他文件 |
|---|---|---|
| 閒置超時 | `contracts/stc_mode.h` 的 `CHAT 閒置 N s` | 只可引用，數值必須相同 |
| 契約版本 | `stc_mode.h` 的 `contract vX.Y.Z` | `docs/mode-gate.md` 標題必須相同 |

由 `tools/check_mode_gate.py` 把關，CI 會跑。改一邊不改另一邊，build 直接紅。

## 閘 4 — 本地驗證（攔「以為改好」）

```bash
python3 tools/check_docs.py
python3 tools/check_mode_gate.py
python3 -m unittest discover -s tools/tests -v
grep -rn "90 s\|90000" docs/ contracts/      # 舊值要清零
```

最後那條 grep 是關鍵：改數字時，最容易漏的是註解與驗收表。**舊值仍在 = 未改完。**

## 閘 5 — PR 描述對得上 diff（攔「描述說謊」）

PR body 內每一個數字、檔名、版本號，都要能在 `git diff` 找到對應。合併後如果 body 仍是舊版本，就要即時改：

```bash
gh pr edit <N> --body "..."
```

今日的實例：diff 已是 60 s，body 仍寫「60 s -> 90 s」「註解改 90 s」。這種不一致會令下一個接手的人照錯的描述去理解。

## 閘 6 — 收尾一致性（攔「留下孤兒改動」）

一個改動只可以存在於一個地方。如果它已經在新 PR：

- 舊分支上要有 revert commit（不是靜靜留著）
- issue 上要留決定記錄（今次：issue #45 原寫 90 s，實際決定 60 s）
- `CHANGELOG.md` 的 `[Unreleased]` 要有一條對應

---

## 檢查清單（貼入 PR body 用）

```markdown
Closes #<issue，不是 PR>

## 改動
- <每一項都對得上 diff>

## 沒有改
- <明確列出刻意不動的東西>

## 驗證
- [ ] python3 tools/check_docs.py
- [ ] python3 tools/check_mode_gate.py
- [ ] python3 -m unittest discover -s tools/tests -v
- [ ] grep 舊值已清零
- [ ] PR body 內每個數字都與 diff 一致
```

---

## 為什麼要六道而不是三道

三道（改完、跑測試、開 PR）聽起來更爽，但今日四個錯全部發生在「改完」與「開 PR」之間：

- 對象錯 → 閘 1、2
- 數字不一 → 閘 3、4
- 描述不符 → 閘 5
- 孤兒改動 → 閘 6

寧願每一步多花三十秒，也不要事後重做一次。
