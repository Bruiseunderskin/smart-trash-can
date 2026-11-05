# chat server（B1）

`POST /ask`：收 PCM 16k/16-bit/mono，回一行 JSON header ＋ chunked PCM。

## 跑起

```bash
cd server/chat
python3 -m venv .venv && . .venv/bin/activate
pip install -r requirements.txt
cp .env.example .env      # 只改 DEVICE_TOKEN；上游 key 用 vault 注入
STUB=1 uvicorn main:app --host 0.0.0.0 --port 8080
```

`STUB=1`：不呼叫任何上游，回固定回覆 ＋ `assets/` 內的 WAV（沒有就用 0.5 s 靜音）。
這個模式就是分階段計劃的 B1-a，裝置端今日就可以接。

## 環境變數（只列名，值不入 repo）

| 變數 | 用途 |
|---|---|
| `DEVICE_TOKEN` | 裝置呼叫本服務的憑證（配網時寫入裝置 NVS） |
| `ASR_KEY` / `ASR_REGION` | 粵語 ASR（需支援 `yue-HK`） |
| `LLM_KEY` / `LLM_BASE` / `LLM_MODEL` | OpenAI 相容端點 |
| `TTS_KEY` | 粵語 TTS |
| `STUB` | `1` = 全部用假實作 |

## 協議

請求 `multipart/form-data`：

- `meta`：`{"device":"stc-01","round":3,"lang":"yue","max_ms":8000}`
- `audio`：PCM 16 kHz / 16-bit / mono（raw）

回應：`application/octet-stream`，第一行 JSON：

```
{"text":"...","reply":"...","sr":16000,"fmt":"s16le","ms":1850}
```

之後是 PCM bytes。任何階段超時 → header 加 `"fallback":1`，內容改為預錄答案。

## 驗收（對應 docs/ai-chat-partition.md §4.5）

- B1-a：`STUB=1` 時裝置收到並播出聲音
- B1-b：20 條小朋友風格語音，WER ≤ 20%
- B1-c：20 條問答，離題 < 10%
- B1-d：首字 p95 ≤ 2.5 s
- B1-e：拔網 3 s 內回落 B0
