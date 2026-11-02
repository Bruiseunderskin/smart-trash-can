## 補充：最新 SC101IOT 四類現場測試結果

這是在模型訓練、ONNX 匯出、量化與選模全部鎖定後，使用 SC101IOT 現場
拍攝的獨立 smoke set。四張圖片均未用於訓練、量化校準或模型選模。

測試時間：2026-11-02 23:26–23:31（Asia/Shanghai）  
相機：SC101IOT，1280×720 UYVY 經板上 JPEG 編碼及 COM5 下載  
低信心門檻：0.70

| Truth | 實物 | Prediction | Confidence | Handling |
|---|---|---|---:|---|
| 其他 | 紙巾 | 可回收物 | 0.815399 | 錯誤，被接受 |
| 可回收物 | 紙盒 | 其他 | 0.875398 | 錯誤，被接受 |
| 廚餘 | 橙子 | 廚餘 | 0.580522 | 正確，但 abstain |
| 有害 | 藥品包裝 | 其他 | 0.488199 | 錯誤，abstain |

逐類結果：

| Class | Precision | Recall | F1 | Support |
|---|---:|---:|---:|---:|
| 其他 | 0.000000 | 0.000000 | 0.000000 | 1 |
| 可回收物 | 0.000000 | 0.000000 | 0.000000 | 1 |
| 廚餘 | 1.000000 | 1.000000 | 1.000000 | 1 |
| 有害 | 0.000000 | 0.000000 | 0.000000 | 1 |

- Closed-set accuracy：`0.250000`
- Closed-set macro-F1：`0.250000`
- Confusion matrix：`[[0,1,0,0],[1,0,0,0],[0,0,1,0],[1,0,0,0]]`
- Abstain：`2`
- Coverage：`0.500000`
- Accepted accuracy：`0.000000`

證據摘要：

- 評估圖片 manifest SHA-256：
  `9ca04fc373d016b173ca1655f0bdfecb592af781c1a452a698b0e5365308c362`
- ONNX SHA-256：
  `0447a566cdbf6f45a43245205e5667f592d0f6d86bc1c7067bf7e736e6e8177f`
- 本機證據：`docs/issue12/live-camera/metrics.json`、
  `docs/issue12/live-camera/metrics.md`、
  `docs/issue12/live-camera/captures.json`
- 本地證據尚未 commit/push，所以此評論不提供失效的 GitHub artifact 連結。

解讀：

1. 現有模型在此 4 張真實相機圖片上只有 `25%` closed-set accuracy。
2. 兩張錯誤的 confidence 分別為 `0.815399` 與 `0.875398`，證明目前
   0.70 threshold 無法防止高信心錯誤。
3. 兩張被 abstain 的圖片中，一張分類正確、一張錯誤；不能把 abstain
   本身等同於安全。
4. 每類只有 1 張，不能把 `0.25` 當成穩定的總體精度估計；但它已足以
   否決「目前模型可直接量產或自動開蓋」的結論。

建議：

- 保持 issue #12 為驗證完成，但另開模型改善／擴大現場評估的 follow-up。
- 建立每類至少 30 張、涵蓋至少 10 個獨立物件與多背景、光照、距離、
  角度的 locked field set。
- 增加固定 ROI／物件偵測裁切，並補足類似真實使用場景的訓練資料。
- 不要用這 4 張評估圖調 threshold、挑模型或校準；重新訓練後再用新的
  locked field set 驗證。
- 在現場 accuracy 改善前，自動開蓋必須有人工確認或更可靠的多幀／
  距離感測防護。
