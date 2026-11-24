# 智分桶 EDU — 遊戲音效組

六個短音效，全部**原創合成**（正弦／鐘聲音色），無第三方素材、唔需要標註、無授權限制。
格式同 S31 現有 `AudioService::PlaySound()` 完全對應：**Ogg 容器、Opus 編碼、48 kHz、單聲道**。

| 檔案 | 觸發時機 | 長度 | 大小 | 音色 |
|---|---|---|---|---|
| `sfx-scan.ogg` | 按下掃描、鏡頭開 | 0.17 s | 2.1 KB | 高音單響 |
| `sfx-decide.ogg` | 判斷完成、出類別 | 0.31 s | 1.9 KB | 向上三音 |
| `sfx-correct.ogg` | 答對、入正確桶 | 0.56 s | 3.8 KB | 亮琶音（頻譜重心 2.5 kHz） |
| `sfx-wrong.ogg` | 答錯 | 0.63 s | 4.8 KB | 低沉雙響（412 Hz，唔刺耳） |
| `sfx-retry.ogg` | 掃唔清，提示再放 | 0.51 s | 5.2 KB | 柔和中音兩下 |
| `sfx-round.ogg` | 五回合結束、結算 | 1.61 s | 11.4 KB | 和弦號角 |

全部峰值 ≤ 0.88，無削波。`sfx-preview.mp3` 係六個順序播放嘅試聽檔。

## 用法

放喺 `assets` 分區，經 `Assets::GetAssetData()` 取 mmap 指標，再交俾 `PlaySound()`：

```cpp
const void* ptr = nullptr;
size_t len = 0;
if (assets->GetAssetData("sfx-correct.ogg", ptr, len)) {
    audio_service_.PlaySound(
        std::string_view(static_cast<const char*>(ptr), len));
}
```

`PlaySound()` 內部已處理 codec 上電、Ogg demux、逐包推 decode queue，唔使另開 I2S。

## 音量

- 音效同綠豆語音同時出現時，音效相對語音 **−12 dB**。
- 小喇叭低頻弱，`sfx-wrong` 用 174–196 Hz 已經夠沉，唔需要再壓低。
- 連續答對時，第二個音效延遲 120 ms 再播，避免疊聲。

## 重新生成

`make_sfx.py`（Python + numpy + ffmpeg）可以改頻率、長度、衰減再產生一次：

```bash
ffmpeg -i in.wav -ac 1 -ar 48000 -c:a libopus -b:a 48k -application audio out.ogg
```

單聲道係硬要求：`OggDemuxer` 見到立體聲會直接拒收。
