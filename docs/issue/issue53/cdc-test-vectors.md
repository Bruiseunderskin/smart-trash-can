# CDC 線上格式與測試案例草案

讀者：S31／S3 通訊實作者。何時讀：實作或審查 I7。此文件只提供已描述的格式與待確認測試，未批准的恢復策略見 [決策](decisions.md)。

## 已指定的 v1 欄位

16 bytes：`sof ver type seq bin action tof_mm[0..3] crc16`；前六欄各 1 byte，四個距離各 2 bytes，CRC 2 bytes；16-bit 小端，CRC-16/CCITT-FALSE 覆蓋 bytes 0–13。切勿把 USB read 邊界當 frame 邊界。

## 已計算的示例（非硬件證據）

CRC-16/CCITT-FALSE 標準輸入 ASCII `123456789` 的 CRC 為 `29b1`。一個命令草案 payload `a5 01 00 2a 01 01 00 00 00 00 00 00 00 00` 的 CRC 數值為 `8c5d`，小端完整 frame 為 `a5 01 00 2a 01 01 00 00 00 00 00 00 00 00 5d 8c`。實作者須以獨立實作和雙端測試核對，勿把此文件誤作已執行的測試。

## 須覆蓋的案例

- 正常 command、ACK、state；單 bytes 分段、兩幀同讀、前置垃圾、壞 CRC、未知版本及截斷後重同步。
- 遺失 ACK、重送同 seq、同 seq 不同內容、逾時後遲到 ACK、序號 255→0、斷線與節點重啟。
- 對每案例記錄 mock clock 時間、傳送幀、執行次數、ACK 次數、計時器是否重設、預期 fault 及決策來源。
- 去重、session 和恢復政策尚待 D06；任何草案不應變成未經批准的實體安全行為。ACK 不證明蓋已到位。
