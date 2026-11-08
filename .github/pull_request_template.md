Closes #<issue 編號；不是 PR 編號>

## 改動

- <一項一行，每一項都對得上 diff>

## 沒有改

- <刻意不動的東西，避免 reviewer 猜>

## 驗證

- [ ] `python3 tools/check_docs.py`
- [ ] `python3 tools/check_mode_gate.py`
- [ ] `python3 -m unittest discover -s tools/tests -v`
- [ ] grep 舊值已清零（改數字時）
- [ ] `CHANGELOG.md` 的 `[Unreleased]` 有一條對應
- [ ] PR body 內每個數字與 diff 一致

## 影響

- <其他分支是否需要 rebase；是否改到契約或模型>
