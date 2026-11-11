# 模型產物

讀者：模型及韌體整合人員。何時讀：取用分類模型前。

| 目錄 | 狀態 | 說明 |
|---|---|---|
| [wordless-padded](wordless-padded/README.md) | **現行** | 158 張卡面（38 印刷 ＋ 120 輔助）、珠光紙 sprite、每卡 200 張場景、S31 int8 `.espdl`；同時係資料集唯一 source of truth |

`mixed-card-object/` 已移除（2026-11-11）。舊混合模型同舊無字候選都由上方 `wordless-padded` 取代；
需要舊檔時用 `git show <commit>:models/mixed-card-object/...`。

[上層指南](../README.md) · [專案入口](../README.md)
