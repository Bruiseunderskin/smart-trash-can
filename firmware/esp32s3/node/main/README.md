# stc_node/main — 節點原始碼

讀者：改節點韌體的人。何時讀：加蓋、改腳位或改 CDC 幀前。

| 檔案 | 責任 |
|---|---|
| `node_main.c` | 初始化 I²C、命令處理、100 ms `NODE_STATE` 迴圈 |
| `pca9685.c/.h` | PCA9685 `0x40`，50 Hz，四通道脈寬 |
| `tca9548a.c/.h` | TCA9548A `0x70`，一次只選一個通道 |
| `tof050c.c/.h` | TOF050C（VL6180X）`0x29` 讀距 |
| `cdc_dev.c/.h` | USB Serial/JTAG CDC 讀寫 |

幀定義用 `firmware/common/stc_lidmsg.h`（兩個專案編譯同一份）。
