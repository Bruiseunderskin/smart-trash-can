# voice — GAME 模式粵語台詞

讀者：語音／韌體整合人員。何時讀：要播台詞、改句子或重新錄音時。

所有台詞均為 **16 kHz、單聲道、16-bit signed little-endian、無檔頭 raw PCM**，直接餵 16 kHz codec。GAME 模式不連雲端；CHAT 模式才使用 xiaozhi 的對話通道。

## 台詞表

| key | 台詞 | 檔案 | 長度 |
| --- | --- | --- | ---: |
| `open` | 請將卡片放入任何一個桶 | `stc_open.pcm` | 3.288 s |
| `correct` | 答對喇 | `stc_correct.pcm` | 1.872 s |
| `wrong` | 答錯喇 | `stc_wrong.pcm` | 1.872 s |
| `timeout` | 冇放入，下一回合 | `stc_timeout.pcm` | 2.664 s |
| `retry` | 睇唔清，再試一次 | `stc_retry.pcm` | 2.856 s |
| `summary` | 呢局完結 | `stc_summary.pcm` | 1.872 s |
| `cheer_correct_a` | 哇，你好叻啊！再接再厲！ | `stc_cheer_correct_a.pcm` | 4.320 s |
| `cheer_correct_b` | 繼續加油！ | `stc_cheer_correct_b.pcm` | 1.992 s |
| `encourage_wrong_a` | 唔緊要，再試一次！ | `stc_encourage_wrong_a.pcm` | 2.760 s |
| `encourage_wrong_b` | 加油，下一題再嚟！ | `stc_encourage_wrong_b.pcm` | 2.784 s |
| `summary_tip` | 記住先分類再投放。 | `stc_summary_tip.pcm` | 2.952 s |

`phrases.json` 是這張表的機器可讀版本（key → text + pcm 檔名），產生工具讀它。

## 播放規則

- `correct`／`wrong`：第一次或未形成連續時使用基本句。
- 連續答對 `streak >= 2`：由 `cheer_correct_a/b` 輪替播放。
- 連續答錯第 2 次起：由 `encourage_wrong_a/b` 輪替播放。
- 輪替以 `round_id` 決定，避免每次都重複同一句，亦令 host tests 可重現。
- 結算先播 `summary`，再排隊播 `summary_tip`。
- 診斷命令 `stc cheer` 會順序播五句新語音，用嚟上板試音。
- 音效由 Ogg/Opus 路徑播放；台詞只可由 raw PCM 路徑播放，不可把 Ogg 當 PCM。

## 來源與格式

台詞使用 Edge TTS `zh-HK-HiuMaanNeural` 合成，再轉為 16 kHz mono raw PCM。新增嘅鼓勵句於 2026-11-24 用 edge-tts 7.3.1 生成；7.2.x 會遇到微軟 `Sec-MS-GEC` 403。

目前 hash（頭 8 位；完整值可用 `sha256sum` 重算）：

```text
e2d092ae  stc_cheer_correct_a.pcm
ab17296c  stc_cheer_correct_b.pcm
1e87f23d  stc_correct.pcm
7226816c  stc_encourage_wrong_a.pcm
1b916b29  stc_encourage_wrong_b.pcm
83d41d2b  stc_open.pcm
0dc356f2  stc_retry.pcm
b064b39e  stc_summary.pcm
54b5f705  stc_summary_tip.pcm
da68dabe  stc_timeout.pcm
202f776a  stc_wrong.pcm
```

## 重新產生

```bash
# 由現成錄音（每個 key 一個 <key>.wav，須為 16 kHz 單聲道 16-bit）
python3 tools/generate_stc_voice_assets.py \
  --phrases firmware/assets/voice/phrases.json \
  --output-dir firmware/assets/voice \
  --source-dir path/to/wav

# 或由 Edge TTS 合成（需 edge-tts 7.3.1+ 與 ffmpeg）
python3 tools/generate_stc_voice_assets.py \
  --phrases firmware/assets/voice/phrases.json \
  --output-dir firmware/assets/voice \
  --tts-voice zh-HK-HiuMaanNeural \
  --keys cheer_correct_a cheer_correct_b encourage_wrong_a encourage_wrong_b summary_tip
```

## 限制

- 這些是整句預錄，不是任意文字 TTS。句子拼接仍不屬 GAME 模式。
- 本地播放為 16 kHz mono；`Es8389AudioCodec::Write()` 只做一次 mono→stereo 複製。
- 播放 pacing 用 40 ms PCM chunk，減少 20 ms 邊界造成嘅 scheduling jitter。