# voice — GAME 模式粵語台詞

讀者：語音／韌體整合人員。何時讀：要播台詞、改句子或重新錄音時。

六句台詞，全部 **16 kHz、單聲道、16-bit signed little-endian、無檔頭 raw PCM**，直接餵 I²S。

| key | 台詞 | 檔案 | 位元組 |
| --- | --- | --- | ---: |
| `open` | 請將卡片放入任何一個桶 | `stc_open.pcm` | 105216 |
| `correct` | 答對喇 | `stc_correct.pcm` | 59904 |
| `wrong` | 答錯喇 | `stc_wrong.pcm` | 59904 |
| `timeout` | 冇放入，下一回合 | `stc_timeout.pcm` | 85248 |
| `retry` | 睇唔清，再試一次 | `stc_retry.pcm` | 91392 |
| `summary` | 呢局完結 | `stc_summary.pcm` | 59904 |

`phrases.json` 是這張表的機器可讀版本（key → text + pcm 檔名），產生工具讀它。

## 來源

素材自 PR #41 分支（`codex/issue25-voice-chat`，commit `02d7f81`）抽出，原位置在該分支的 S31 板目錄 `assets/`。抽取決定見 [docs/issue25](../../../docs/issue/issue25/README.md)：只保留語音素材與產生工具，該分支的狀態機與自製畫面不採用。

音色與錄音來源未在該分支記錄；重新產生時要自己指定 voice（見下）。以下 hash 供辨識是否被改過：

```
1e87f23d…  stc_correct.pcm
83d41d2b…  stc_open.pcm
f0303f83…  stc_retry.pcm
b064b39e…  stc_summary.pcm
da68dabe…  stc_timeout.pcm
202f776a…  stc_wrong.pcm
15d8106f…  phrases.json
```

## 重新產生

```bash
# 由現成錄音（每個 key 一個 <key>.wav，須為 16 kHz 單聲道 16-bit）
python3 tools/generate_stc_voice_assets.py \
  --phrases firmware/assets/voice/phrases.json \
  --output-dir firmware/assets/voice \
  --source-dir path/to/wav

# 或由 Edge TTS 合成（需 edge-tts 與 ffmpeg）
python3 tools/generate_stc_voice_assets.py \
  --phrases firmware/assets/voice/phrases.json \
  --output-dir firmware/assets/voice \
  --tts-voice zh-HK-HiuMaanNeural
```

工具會驗證取樣率／聲道／位元寬，不符即報錯，不會靜靜產生錯格式的檔案。測試見 `tools/tests/test_stc_voice_assets.py`。

## 限制

- 這六句是「整句預錄」，不是任意組合。`contracts/stc_voice.h` 的 `stc_voice_say()` 按 `stc_line_t` 取值；要做句子拼接（14 段組合）屬 issue #25 的 M3，尚未實作。
- 未在 S31 實機驗證過播放（素材來自分支建置，見 [docs/issue25](../../../docs/issue/issue25/README.md)）。
