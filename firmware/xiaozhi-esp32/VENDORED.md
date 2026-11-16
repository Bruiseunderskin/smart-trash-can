# VENDORED — 這個目錄不是本專案寫的

這是上游 **[Xiaozhi-ESP32](https://github.com/78/xiaozhi-esp32)** 的整份原始碼（MCP-based 語音對話框架），
原樣放進來當我們 S31 語音助理的基礎。**1,732 個檔案，佔全 repo 已追蹤檔案的 53%。**

## 對 agent 的硬規則

1. **不要把這裡當成自家程式碼改。** 需要上游沒有的行為，先確認上游有沒有，再看能不能在
   `firmware/common/` 或 `firmware/esp32s31/` 用薄封裝解決。
2. **非改不可時**，在同一個 commit 的 CHANGELOG 條目寫明「改了 vendored 的哪個檔案、為什麼不能在上游解決」，
   否則下次同步上游時改動會無聲消失。
3. 這裡沒有 `.git`，所以**不能** `git submodule update`。同步上游靠手動 diff，不是靠 git。
4. `.gitattributes` 已把 `firmware/xiaozhi-esp32/**` 標為 `linguist-vendored`：
   GitHub 語言統計與 diff 會當它是第三方，這是刻意的。

## 為什麼是 vendored 而不是 submodule

本 repo 每天有兩個 owner 加 AI agent 在建置。submodule 會令 `git clone` 之後還要多一步
`git submodule update --init --recursive`，而且 submodule 內的改動在 `git status` 不會顯示，
對「改完就要燒機」的流程來說是陷阱。9.5 MB 的體積不值得換這個風險。

## 授權

見同目錄 `LICENSE`（上游原檔）。本專案的授權聲明在 repo root。
