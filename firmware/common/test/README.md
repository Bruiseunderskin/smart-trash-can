# firmware/common/test — host 單元測試

讀者：跑測試與改模組的人。何時讀：加測試或看 CI 紅燈前。

```bash
cmake -S firmware/common/test -B build/common-test -DCMAKE_BUILD_TYPE=Debug
cmake --build build/common-test -j
ctest --test-dir build/common-test --output-on-failure
```

只依賴 libc 與 gcc；不需要 LVGL、ESP-IDF 或硬件。每個測試檔對應 `#53` 的一個驗收項：

| 測試 | 驗什麼 |
|---|---|
| `test_contract_ui` | `contracts/stc_ui.h` 的列舉次序與 payload 佈局 |
| `test_classes_order` | `classes.json` 次序 == `models/wordless-padded/labels.txt`，且 bin 值正確 |
