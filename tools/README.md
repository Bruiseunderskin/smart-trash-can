# tools

主機工具：build_dataset.py 準備資料；render_none_scenes.py 生成 `unknown`（冇卡）場景；check_onnx_parity.py 核對 ONNX；quantize_espdl.py 量化；evaluate_ei_tflite.py 與 evaluate_field_probe.py 評估模型。從 repository root 在對應 Python 環境執行各工具 --help，依 requirements 檔安裝所需 runtime。

文件驗證：在 repository root 執行 `python3 tools/check_docs.py`。它只需標準函式庫；涵蓋範圍見 [架構指南](../docs/architecture.md)。

## 本層檔案

- [build_dataset.py](build_dataset.py)
- [check_onnx_parity.py](check_onnx_parity.py)
- [evaluate_ei_tflite.py](evaluate_ei_tflite.py)
- [evaluate_field_probe.py](evaluate_field_probe.py)
- [evaluate_none_scenes.py](evaluate_none_scenes.py)
- [render_none_scenes.py](render_none_scenes.py)
- [evaluate_cards.py](evaluate_cards.py)
- [evaluate_replay.py](evaluate_replay.py)
- [add_unknown_row.py](add_unknown_row.py)
- [render_card_scenes.py](render_card_scenes.py)
- [render_wordless_scenes.py](render_wordless_scenes.py)
- [train_card_classifier.py](train_card_classifier.py)
- [quantize_espdl.py](quantize_espdl.py)
- [requirements-data.txt](requirements-data.txt)
- [requirements-tflite-eval.txt](requirements-tflite-eval.txt)
- [requirements-train-macos-arm64.lock.txt](requirements-train-macos-arm64.lock.txt)
- [requirements-train.txt](requirements-train.txt)

## 子目錄

- [skills](skills/README.md)
- [tests](tests/README.md)

[上層指南](../README.md) · [專案入口](../README.md)
