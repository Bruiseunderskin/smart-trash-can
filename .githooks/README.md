# Git hooks

在 repository root 執行 `python3 tools/dev.py install-hooks` 啟用本機 hooks。Git clone 不會自動安裝 hooks；已有其他 hooksPath 時安裝工具會停止，避免覆蓋設定。

- `pre-commit`：檢查工作目錄及 staged whitespace、README 覆蓋與文件連結。
- `pre-push`：以上檢查、文件及預覽工具測試，以及相對 `origin/main` 變更的第一方韌體編譯。
- Git 沒有原生 pre-PR hook；用 `python3 tools/dev.py pr -- --base main --title "標題" --body-file /path/to/body.md`，通過檢查後才呼叫 `gh pr create`。也可單獨執行 `pre-pr`。

檢查讀取目前工作目錄，不是 staged snapshot；建議提交前保持變更一致。hooks 可被略過，因此 PR CI 是獨立保障。完整環境與覆蓋範圍見 [開發指南](../docs/development.md)。
