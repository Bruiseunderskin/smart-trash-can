# server

場地／開發用的輔助服務。與韌體分開部署，不參與競賽評分。

- `chat/` — AI 對話後端（B1）：裝置錄音 → 本服務 → 雲端 ASR／LLM／TTS → 串流 PCM 回裝置。

所有憑證由環境變數提供，repo 內只保留 `.env.example` 的空值。
