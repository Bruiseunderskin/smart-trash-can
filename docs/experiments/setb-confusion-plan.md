# Set B 誤判：診斷與純模擬驗證計劃

讀者：模型開發者與審查者。何時讀：要重跑「廚餘／藥物／鏡片包裝被判可回收」這條線，或要改判決層參數時。

對應 issue：[#67](https://github.com/Bruiseunderskin/smart-trash-can/issues/67)。症狀由隊友實測提供（Set B 卡），本文件只描述**如何在沒有實卡的情況下量度與修正**。

## 症狀

| 卡 | 真實類別 | 實測被判 |
|---|---|---|
| B-14 至 B-18（果芯、蕉皮、魚骨） | food | recyclable |
| B-22（橙色塑膠藥瓶） | hazardous | recyclable |
| B-28（鏡片濕紙巾鋁箔包） | other | recyclable |

## 已知的三個結構性原因

1. **先驗偏置**：印刷卡 38 張之中可回收 16 張、其餘 7／7／8；每卡 200 個場景，所以場景層面可回收約 2.3 倍。470 張持牌實拍**全部**落在可回收與其他，food 與 hazardous 零實拍。
2. **語意衝突**：藥瓶與鋁箔包在視覺上屬「容器／包裝」，正是可回收類的原型；但它們的標籤由內容決定。單一四路 softmax 只能二選一。
3. **量度缺口**：現有測試是「同一批卡 × 同一個 renderer」，seen 0.998、未見卡 fold-0 0.784。實測失敗落在這段落差內，現有測試無法重現。

## S1 場景矩陣

`tools/render_card_matrix.py`：158 張卡 × 6 條件 × 每格 20 張 = 18,960 張。

| 條件 | 參數 |
|---|---|
| `nominal` | 現有 render 分佈，160×160 |
| `glare` | 1 至 2 個徑向亮斑，峰值 +40% |
| `low_light` | 亮度 ×0.35、高斯噪聲 σ=8 |
| `far` | 卡片縮至畫面 25 至 35%，再放大回 160 |
| `angle` | 旋轉 ±10／20／30 度 |
| `motion` | 方向性模糊，核 7 至 15 px |

```sh
python3 tools/render_card_matrix.py \
  --cards models/wordless-padded/cards \
  --cards-csv models/wordless-padded/cards.csv \
  --out /tmp/card-matrix --per-condition 20
```

輸出 `manifest.csv`（`card_id,class,condition,path,seed`），可逐格重建，不入 git。

## S2 量基線

用 float32 與 `.espdl`（ESP-PPQ 圖模擬）各跑一次，輸出每列 `card_id,truth,food,hazardous,other,recyclable` 的 CSV，再交給判決層工具。

```sh
python3 tools/stc_confusion.py --rows matrix-baseline.csv --tau 0 \
  --min-conf 0.55 --min-margin 0.0 --json-out matrix-baseline.json
```

必看：每卡準確率、top-2 混淆對、ECE、每類 P/R/F1。

## S3 事後先驗校正

用同一批輸出的平均預測分佈估 effective prior（P2P 做法），推論時 `p_i / prior_i^tau` 再歸一。掃 `tau = 0`、`0.5`、`1.0`，同一批場景重測。

```sh
python3 tools/stc_confusion.py --rows matrix-baseline.csv --tau 1.0 \
  --min-conf 0.55 --min-margin 0.15 --json-out matrix-tau1.json
```

校正只改判決層，不改權重；所以可以直接與 `tau=0` 逐卡對比。

## S4 針對性輔助卡

對三對混淆各生成 20 張 Set C 風格卡面（藥盒、鋁箔包、外賣盒、飲品紙包），加進訓練重跑，用 fold-0（未見卡）驗證，避免只在自己造的卡上刷分。

## S5 拒判門檻

掃 `min_conf` 0.45 至 0.80、`min_margin` 0.05 至 0.30，畫「自動化率 對 錯誤率」曲線。產品層要求：寧願拒判（提示再放一次），也不要開錯桶。

## 驗收線

| 項目 | 標準 |
|---|---|
| 印刷 38 卡 `nominal` | ≥ 99% |
| 六條件平均 | ≥ 95% |
| 最差條件 | ≥ 92% |
| 三張已知錯卡（B-14..18、B-22、B-28） | 所有條件 ≥ 95% |
| 自動化率 | ≥ 85% |
| ECE | ≤ 0.05 |

「自動化率」與準確率必須一齊報；只報準確率可以靠大量拒判造假。

## 界線

- 全部數字都是 host 模擬，**不是實機表現**。實機仍要上板驗一次（`docs/` 其他章節的上板流程不變）。
- 模擬修正後要在真實卡上抽驗一張，避免整條模擬管線自說自話。
- 本計劃不改 `contracts/model/classes.json`；類別次序凍結。
