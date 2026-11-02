#!/usr/bin/env python3
"""Evaluate the locked classifier on the independent field-probe set."""

from __future__ import annotations

import argparse
import sys
import hashlib
import json
from dataclasses import dataclass
from pathlib import Path
from typing import Any

import numpy as np
from PIL import Image


MODEL_CLASS_NAMES = ("其他", "可回收物", "廚餘", "有害")
MODEL_TO_BIN_HK = {
    "其他": "其他垃圾",
    "可回收物": "可回收物",
    "廚餘": "廚餘",
    "有害": "有害垃圾",
}
BIN_TO_MODEL_CLASS = {
    "其他垃圾": "其他",
    "可回收物": "可回收物",
    "廚餘": "廚餘",
    "有害垃圾": "有害",
}
LOW_CONFIDENCE_THRESHOLD = 0.70
CROP_FRACTION = 0.60
INPUT_SIZE = 160
EXPECTED_ONNX_SHA256 = "0447a566cdbf6f45a43245205e5667f592d0f6d86bc1c7067bf7e736e6e8177f"
AMBIGUOUS_FILES = {
    "other/miscellaneous-trash_33.jpg",
    "food/vegetation_4.jpg",
}


@dataclass(frozen=True)
class ProbeImage:
    path: Path
    file: str
    truth: str
    source: str
    source_class: str
    sha256_16: str


def load_probe_images(manifest_path: Path, *, expected_count: int = 17) -> list[ProbeImage]:
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    entries = manifest.get("images")
    if not isinstance(entries, list) or len(entries) != expected_count:
        raise ValueError(f"expected {expected_count} field-probe images")

    images = []
    for entry in entries:
        if not isinstance(entry, dict):
            raise ValueError("field-probe manifest entries must be objects")
        file = str(entry.get("file", ""))
        path = manifest_path.parent / file
        if not path.is_file():
            raise FileNotFoundError(path)
        digest = hashlib.sha256(path.read_bytes()).hexdigest()[:16]
        expected_digest = str(entry.get("sha256_16", ""))
        if digest != expected_digest:
            raise ValueError(f"SHA-256 prefix mismatch for {file}")
        bin_hk = str(entry.get("bin_hk", ""))
        if bin_hk not in BIN_TO_MODEL_CLASS:
            raise ValueError(f"unknown Hong Kong bin label: {bin_hk}")
        source_class = str(entry.get("source_class", ""))
        source = "battery" if source_class.casefold() == "batteries" else "realwaste"
        images.append(
            ProbeImage(
                path=path,
                file=file,
                truth=BIN_TO_MODEL_CLASS[bin_hk],
                source=source,
                source_class=source_class,
                sha256_16=digest,
            )
        )
    return images


def center_square_crop(image: Image.Image, fraction: float = 0.60) -> Image.Image:
    if not 0.0 < fraction <= 1.0:
        raise ValueError("crop fraction must be in (0, 1]")
    width, height = image.size
    side = max(1, round(min(width, height) * fraction))
    left = (width - side) // 2
    top = (height - side) // 2
    return image.crop((left, top, left + side, top + side))


def _summary(predictions: list[dict[str, Any]]) -> dict[str, Any]:
    if not predictions:
        raise ValueError("predictions must not be empty")
    count = len(predictions)
    correct = sum(int(prediction["correct"]) for prediction in predictions)
    accepted = [prediction for prediction in predictions if not prediction["abstained"]]
    accepted_correct = sum(int(prediction["correct"]) for prediction in accepted)
    ambiguous = [
        prediction for prediction in predictions if prediction.get("ambiguous", False)
    ]
    ambiguous_misclassified = sum(
        int(not prediction["correct"]) for prediction in ambiguous
    )
    ambiguity_neutral_correct = correct + ambiguous_misclassified
    per_class = {}
    for class_name in MODEL_CLASS_NAMES:
        truth_rows = [prediction for prediction in predictions if prediction["truth"] == class_name]
        predicted_rows = [
            prediction for prediction in predictions if prediction["prediction"] == class_name
        ]
        true_positive = sum(int(prediction["correct"]) for prediction in truth_rows)
        precision = true_positive / len(predicted_rows) if predicted_rows else 0.0
        recall = true_positive / len(truth_rows) if truth_rows else 0.0
        f1 = 2.0 * precision * recall / (precision + recall) if precision + recall else 0.0
        per_class[class_name] = {
            "precision": precision,
            "recall": recall,
            "f1": f1,
            "support": len(truth_rows),
        }
    return {
        "images": count,
        "correct_count": correct,
        "accuracy": correct / count,
        "ambiguity_neutral_correct_count": ambiguity_neutral_correct,
        "ambiguity_neutral_accuracy": ambiguity_neutral_correct / count,
        "ambiguous_count": len(ambiguous),
        "ambiguous_misclassified_count": ambiguous_misclassified,
        "macro_f1": sum(values["f1"] for values in per_class.values())
        / len(MODEL_CLASS_NAMES),
        "abstained_count": count - len(accepted),
        "coverage": len(accepted) / count,
        "accepted_accuracy": accepted_correct / len(accepted) if accepted else 0.0,
        "mean_confidence": sum(
            float(prediction["confidence"]) for prediction in predictions
        )
        / count,
        "per_class": per_class,
    }


def summarize_predictions(predictions: list[dict[str, Any]]) -> dict[str, Any]:
    by_source = {}
    for source in ("realwaste", "battery"):
        source_predictions = [
            prediction for prediction in predictions if prediction["source"] == source
        ]
        if source_predictions:
            by_source[source] = _summary(source_predictions)
    return {"overall": _summary(predictions), "by_source": by_source}


def analyze_background_shortcut(summary: dict[str, Any]) -> dict[str, Any]:
    battery = summary["by_source"]["battery"]
    realwaste = summary["by_source"]["realwaste"]
    confidence_gap = battery["mean_confidence"] - realwaste["mean_confidence"]
    battery_high_confidence_count = (
        battery["images"] - battery["abstained_count"]
    )
    battery_high_confidence_correct_count = round(
        battery_high_confidence_count * battery["accepted_accuracy"]
    )
    signal_detected = (
        battery["accuracy"] == 1.0
        and battery["coverage"] == 1.0
        and realwaste["accuracy"] < battery["accuracy"]
        and confidence_gap >= 0.15
    )
    confidence_bias_signal = (
        battery["coverage"] - realwaste["coverage"] >= 0.20
        and abs(battery["accuracy"] - realwaste["accuracy"]) <= 0.10
    )
    return {
        "signal_detected": signal_detected,
        "confidence_bias_signal": confidence_bias_signal,
        "battery": battery,
        "realwaste": realwaste,
        "battery_high_confidence_count": battery_high_confidence_count,
        "battery_high_confidence_correct_count": battery_high_confidence_correct_count,
        "mean_confidence_gap": confidence_gap,
        "interpretation": (
            "電池產品照全部正確、全部被接受，而且信心明顯高於 RealWaste；"
            "這與模型依賴背景捷徑的假設一致。但電池亦是唯一的有害垃圾來源，"
            "RealWaste 沒有有害垃圾對照，所以本探測集不能把背景與類別/拍攝風格"
            "完全分離。"
            if signal_detected
            else (
                "完整背景捷徑條件（電池全部高分正確、RealWaste 同時大幅崩塌）未成立；"
                f"但電池 {battery_high_confidence_count} 張全部高分並被接受，只有 "
                f"{battery_high_confidence_correct_count} 張正確，而且 coverage 明顯高於 "
                "RealWaste。這顯示白底產品照可推高信心而沒有帶來準確率優勢；"
                "仍不能單憑這個對照證明背景係唯一原因。"
                if confidence_bias_signal
                else "未檢出「電池照片全部高分正確，而 RealWaste 明顯下跌」的簡單背景捷徑訊號。"
            )
        ),
    }


def _metric_delta(before: dict[str, Any], after: dict[str, Any]) -> dict[str, float]:
    return {
        f"{metric}_delta": float(after[metric]) - float(before[metric])
        for metric in ("accuracy", "coverage", "mean_confidence")
    }


def analyze_crop_effect(
    baseline: dict[str, Any],
    cropped: dict[str, Any],
) -> dict[str, Any]:
    overall = _metric_delta(baseline["overall"], cropped["overall"])
    by_source = {
        source: _metric_delta(
            baseline["by_source"][source],
            cropped["by_source"][source],
        )
        for source in ("realwaste", "battery")
    }
    realwaste_gain = by_source["realwaste"]["accuracy_delta"]
    battery_change = abs(by_source["battery"]["accuracy_delta"])
    framing_sensitive = realwaste_gain >= 0.20 and battery_change <= 0.10
    return {
        "overall": overall,
        "by_source": by_source,
        "realwaste_framing_sensitive": framing_sensitive,
        "interpretation": (
            "中心裁切令 RealWaste accuracy 明顯改善，而電池 accuracy 保持穩定，"
            "因此物件位置／比例係可信的影響因素。"
            if framing_sensitive
            else (
                "中心裁切帶來部分 RealWaste 改善，但未達明顯改善門檻；"
                "物件位置／比例有貢獻，但不足以解釋主要錯誤，語意辨認仍是主要問題。"
                if realwaste_gain > 0 and battery_change <= 0.10
                else "中心裁切未出現「RealWaste 明顯改善、電池保持穩定」的來源特定模式，"
                "因此不能把主要錯誤歸因於物件位置／比例。"
            )
        ),
    }


def _softmax(values: np.ndarray) -> np.ndarray:
    shifted = values - values.max(axis=1, keepdims=True)
    exponentials = np.exp(shifted)
    return exponentials / exponentials.sum(axis=1, keepdims=True)


def _predict(
    images: list[ProbeImage],
    session: Any,
    transform: Any,
    *,
    crop: bool,
) -> list[dict[str, Any]]:
    input_name = session.get_inputs()[0].name
    predictions = []
    for image in images:
        with Image.open(image.path) as opened:
            rgb = opened.convert("RGB")
            crop_size = None
            if crop:
                rgb = center_square_crop(rgb, CROP_FRACTION)
                crop_size = rgb.width
            tensor = transform(rgb).numpy()[None].astype(np.float32)
        output = np.asarray(session.run(None, {input_name: tensor})[0], dtype=np.float64)
        if output.ndim != 2 or output.shape != (1, len(MODEL_CLASS_NAMES)):
            raise RuntimeError(f"unexpected ONNX output shape for {image.file}: {output.shape}")
        row = output[0]
        if not (
            np.all(row >= 0)
            and np.all(row <= 1)
            and np.isclose(row.sum(), 1.0, atol=1e-4)
        ):
            row = _softmax(output)[0]
        predicted_index = int(np.argmax(row))
        prediction = MODEL_CLASS_NAMES[predicted_index]
        confidence = float(row[predicted_index])
        predictions.append(
            {
                "file": image.file,
                "sha256_16": image.sha256_16,
                "source": image.source,
                "source_class": image.source_class,
                "truth": image.truth,
                "prediction": prediction,
                "confidence": confidence,
                "correct": prediction == image.truth,
                "abstained": confidence < LOW_CONFIDENCE_THRESHOLD,
                "ambiguous": image.file in AMBIGUOUS_FILES,
                "crop_size": crop_size,
                "probabilities": {
                    class_name: float(row[index])
                    for index, class_name in enumerate(MODEL_CLASS_NAMES)
                },
            }
        )
    return predictions


def _experiment(predictions: list[dict[str, Any]]) -> dict[str, Any]:
    summary = summarize_predictions(predictions)
    return {
        "summary": summary,
        "background_shortcut": analyze_background_shortcut(summary),
        "predictions": predictions,
    }


def _load_comparison(path: Path) -> dict[str, Any]:
    report = json.loads(path.read_text(encoding="utf-8"))
    return {
        "source_path": path.as_posix(),
        "source_sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
        "source_onnx_sha256": report["onnx_sha256"],
        "camera": report.get("camera", "SC101IOT"),
        "images": report["images"],
        "accuracy": report["closed_set"]["accuracy"],
        "macro_f1": report["closed_set"]["macro_f1"],
        "abstained_count": report["low_confidence"]["abstained_count"],
        "coverage": report["low_confidence"]["coverage"],
        "accepted_accuracy": report["low_confidence"]["accepted_accuracy"],
    }


def _source_label(source: str) -> str:
    return "電池" if source == "battery" else "RealWaste"


def _yes_no(value: bool) -> str:
    return "yes" if value else "no"


def _summary_row(name: str, summary: dict[str, Any]) -> str:
    return (
        f"| {name} | {summary['images']} | {summary['accuracy']:.6f} | "
        f"{summary['macro_f1']:.6f} | {summary['abstained_count']} | "
        f"{summary['coverage']:.6f} | {summary['accepted_accuracy']:.6f} | "
        f"{summary['mean_confidence']:.6f} |"
    )


def _prediction_rows(predictions: list[dict[str, Any]]) -> list[str]:
    return [
        (
            f"| {prediction['file']} | {MODEL_TO_BIN_HK[prediction['truth']]} | "
            f"{prediction['prediction']} | {prediction['confidence']:.6f} | "
            f"{_yes_no(prediction['correct'])} | {_yes_no(prediction['abstained'])} | "
            f"{_source_label(prediction['source'])} | "
            f"{_yes_no(prediction['ambiguous'])} |"
        )
        for prediction in predictions
    ]


def render_markdown(report: dict[str, Any]) -> str:
    baseline = report["experiments"]["baseline"]
    cropped = report["experiments"]["center_crop_60"]
    baseline_summary = baseline["summary"]
    cropped_summary = cropped["summary"]
    background = report["analysis"]["background_shortcut"]
    crop_effect = report["analysis"]["crop_effect"]
    comparison = report["comparison"]["sc101iot"]

    lines = [
        "# Issue #12：17 張獨立 field-probe 評估",
        "",
        "> **不可作為現場準確率估計。** 每類只有 2–8 張，而且來源不同；"
        "本結果只回答同一鎖定模型換資料來源後錯在哪裡。",
        "",
        f"- ONNX：`{report['model']['path']}`",
        f"- ONNX SHA-256：`{report['model']['sha256']}`",
        f"- Manifest SHA-256：`{report['manifest']['sha256']}`",
        f"- 固定 low-confidence threshold：`{report['threshold']:.2f}`",
        f"- 中心裁切比例：`{report['crop_fraction']:.0%}`（以短邊建立中心正方形）",
        "- 紀律：本批圖永久排除於訓練、校準、閾值掃描及模型選擇；"
        "模型與 threshold 均未改動。",
        "",
        "## Baseline：原始圖片",
        "",
        "| Dataset | Images | Accuracy | Macro-F1 | Abstain | Coverage | "
        "Accepted accuracy | Mean confidence |",
        "|---|---:|---:|---:|---:|---:|---:|---:|",
        _summary_row("全部", baseline_summary["overall"]),
    ]
    for source in ("realwaste", "battery"):
        if source in baseline_summary["by_source"]:
            lines.append(_summary_row(_source_label(source), baseline_summary["by_source"][source]))
    lines.extend(
        (
            "",
            "分佈不均而且電池只有一個真值類別，所以 Macro-F1 只用來保留完整數字，"
            "不應跨來源比較；正確性以逐張結果為準。",
            "",
            "| file | bin_hk | prediction | confidence | correct | abstained | source | ambiguous |",
            "|---|---|---|---:|---|---|---|---|",
            *_prediction_rows(baseline["predictions"]),
            "",
            "## 背景捷徑檢查",
            "",
            f"- 電池 3 張 accuracy："
            f"`{baseline_summary['by_source']['battery']['accuracy']:.6f}`；"
            f"coverage：`{baseline_summary['by_source']['battery']['coverage']:.6f}`；"
            f"平均信心：`{baseline_summary['by_source']['battery']['mean_confidence']:.6f}`。",
            f"- RealWaste 14 張 accuracy："
            f"`{baseline_summary['by_source']['realwaste']['accuracy']:.6f}`；"
            f"coverage：`{baseline_summary['by_source']['realwaste']['coverage']:.6f}`；"
            f"平均信心：`{baseline_summary['by_source']['realwaste']['mean_confidence']:.6f}`。",
            f"- 簡單捷徑訊號：`{_yes_no(background['signal_detected'])}`。",
            f"- 白底高信心偏差訊號："
            f"`{_yes_no(background.get('confidence_bias_signal', False))}`；"
            f"電池高分張數：`{background.get('battery_high_confidence_count', 0)}`；"
            f"其中正確：`{background.get('battery_high_confidence_correct_count', 0)}`。",
            f"- 判讀：{background['interpretation']}",
            "",
            "## 中心 60% 裁切對照",
            "",
            "| Dataset | Images | Accuracy | Macro-F1 | Abstain | Coverage | "
            "Accepted accuracy | Mean confidence |",
            "|---|---:|---:|---:|---:|---:|---:|---:|",
            _summary_row("全部", cropped_summary["overall"]),
        )
    )
    for source in ("realwaste", "battery"):
        if source in cropped_summary["by_source"]:
            lines.append(_summary_row(_source_label(source), cropped_summary["by_source"][source]))
    lines.extend(
        (
            "",
            "| 對照項 | Baseline accuracy | 裁切 accuracy | Δ | "
            "Baseline coverage | 裁切 coverage | Δ |",
            "|---|---:|---:|---:|---:|---:|---:|",
        )
    )
    deltas = crop_effect
    for name, summary in (
        ("全部", cropped_summary["overall"]),
        ("RealWaste", cropped_summary["by_source"]["realwaste"]),
        ("電池", cropped_summary["by_source"]["battery"]),
    ):
        source_key = "overall" if name == "全部" else (
            "realwaste" if name == "RealWaste" else "battery"
        )
        before = (
            baseline_summary["overall"]
            if source_key == "overall"
            else baseline_summary["by_source"][source_key]
        )
        delta_set = (
            deltas["overall"]
            if source_key == "overall"
            else deltas["by_source"][source_key]
        )
        lines.append(
            f"| {name} | {before['accuracy']:.6f} | {summary['accuracy']:.6f} | "
            f"{delta_set['accuracy_delta']:+.6f} | "
            f"{before['coverage']:.6f} | {summary['coverage']:.6f} | "
            f"{delta_set['coverage_delta']:+.6f} |"
        )
    lines.extend(
        (
            "",
            "| file | bin_hk | prediction | confidence | correct | abstained | source | ambiguous |",
            "|---|---|---|---:|---|---|---|---|",
            *_prediction_rows(cropped["predictions"]),
            "",
            f"判讀：{crop_effect['interpretation']}",
            "",
            "## 與 SC101IOT 4 張現場圖對照",
            "",
            "| Dataset | Images | Accuracy | Macro-F1 | Coverage | Accepted accuracy |",
            "|---|---:|---:|---:|---:|---:|",
            f"| SC101IOT live | {comparison['images']} | {comparison['accuracy']:.6f} | "
            f"{comparison['macro_f1']:.6f} | {comparison['coverage']:.6f} | "
            f"{comparison['accepted_accuracy']:.6f} |",
            f"| Field probe baseline | {baseline_summary['overall']['images']} | "
            f"{baseline_summary['overall']['accuracy']:.6f} | "
            f"{baseline_summary['overall']['macro_f1']:.6f} | "
            f"{baseline_summary['overall']['coverage']:.6f} | "
            f"{baseline_summary['overall']['accepted_accuracy']:.6f} |",
            f"| Field probe center crop 60% | {cropped_summary['overall']['images']} | "
            f"{cropped_summary['overall']['accuracy']:.6f} | "
            f"{cropped_summary['overall']['macro_f1']:.6f} | "
            f"{cropped_summary['overall']['coverage']:.6f} | "
            f"{cropped_summary['overall']['accepted_accuracy']:.6f} |",
            "",
            "兩組圖片數、類別分佈和拍攝來源都不同，只可比較同一模型換域後的方向和逐張錯誤，"
            "不可把差異當成穩定的現場準確率。",
            "",
            "## 爭議標註",
            "",
            "- `other/miscellaneous-trash_33.jpg`：乾果殼可爭議屬廚餘或其他；"
            "逐張表已標 `ambiguous=yes`，不以這張單獨判模型錯。",
            "- `food/vegetation_4.jpg`：整條青瓜與廚餘殘渣分佈不同；"
            "逐張表已標 `ambiguous=yes`，不以這張單獨判模型錯。",
            "",
            "嚴格版本仍照 manifest 原標籤計入 accuracy，沒有事後改真值；"
            "上述兩張只作人工爭議標記。",
            f"Baseline 嚴格 accuracy：`{baseline_summary['overall']['accuracy']:.6f}`；"
            f"把爭議圖的錯判視為「不確定／不歸咎模型」後的靈敏度 accuracy："
            f"`{baseline_summary['overall'].get('ambiguity_neutral_accuracy', 0.0):.6f}`。",
            f"裁切後嚴格 accuracy：`{cropped_summary['overall']['accuracy']:.6f}`；"
            f"爭議靈敏度 accuracy："
            f"`{cropped_summary['overall'].get('ambiguity_neutral_accuracy', 0.0):.6f}`。"
            "這兩個靈敏度數字只用於分析，不取代嚴格 manifest 指標。",
            "",
        )
    )
    if report.get("load_failures"):
        lines.extend(("## 載入失敗", ""))
        for failure in report["load_failures"]:
            lines.append(f"- `{failure['file']}`：{failure['error']}")
        lines.append("")
    return "\n".join(lines)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--manifest",
        type=Path,
        default=Path("data/field-probe/manifest.json"),
    )
    parser.add_argument(
        "--onnx",
        type=Path,
        default=Path("runs/issue12/cls160-grouped/weights/best.onnx"),
    )
    parser.add_argument(
        "--sc101iot-metrics",
        type=Path,
        default=Path("docs/issue12/live-camera/metrics.json"),
    )
    parser.add_argument(
        "--out-json",
        type=Path,
        default=Path("docs/issue12/field-probe/metrics.json"),
    )
    parser.add_argument(
        "--out-md",
        type=Path,
        default=Path("docs/issue12/field-probe/metrics.md"),
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    onnx_sha256 = hashlib.sha256(args.onnx.read_bytes()).hexdigest()
    if onnx_sha256 != EXPECTED_ONNX_SHA256:
        raise ValueError(
            f"ONNX SHA-256 is {onnx_sha256}, expected locked model {EXPECTED_ONNX_SHA256}"
        )
    comparison = _load_comparison(args.sc101iot_metrics)
    if comparison["source_onnx_sha256"] != onnx_sha256:
        raise ValueError("SC101IOT comparison report used a different ONNX model")

    images = load_probe_images(args.manifest)
    counts_by_bin: dict[str, int] = {}
    for image in images:
        bin_hk = MODEL_TO_BIN_HK[image.truth]
        counts_by_bin[bin_hk] = counts_by_bin.get(bin_hk, 0) + 1

    import onnxruntime as ort
    from ultralytics.data.augment import classify_transforms

    session = ort.InferenceSession(str(args.onnx), providers=["CPUExecutionProvider"])
    transform = classify_transforms(INPUT_SIZE)
    baseline = _experiment(_predict(images, session, transform, crop=False))
    cropped = _experiment(_predict(images, session, transform, crop=True))

    report = {
        "schema_version": 1,
        "purpose": "Independent field probe; never used for training, calibration, "
        "threshold selection, or model selection",
        "model": {
            "path": args.onnx.as_posix(),
            "sha256": onnx_sha256,
            "bytes": args.onnx.stat().st_size,
            "locked": True,
        },
        "manifest": {
            "path": args.manifest.as_posix(),
            "sha256": hashlib.sha256(args.manifest.read_bytes()).hexdigest(),
            "images": len(images),
            "counts_by_bin": counts_by_bin,
            "counts_by_source": {
                "realwaste": sum(image.source == "realwaste" for image in images),
                "battery": sum(image.source == "battery" for image in images),
            },
        },
        "input_size": INPUT_SIZE,
        "preprocessing": "Ultralytics classify_transforms(160): resize short side, "
        "center crop, RGB, ImageNet normalization",
        "threshold": LOW_CONFIDENCE_THRESHOLD,
        "crop_fraction": CROP_FRACTION,
        "experiments": {
            "baseline": baseline,
            "center_crop_60": cropped,
        },
        "analysis": {
            "background_shortcut": baseline["background_shortcut"],
            "crop_effect": analyze_crop_effect(
                baseline["summary"],
                cropped["summary"],
            ),
        },
        "comparison": {"sc101iot": comparison},
        "ambiguous_files": sorted(AMBIGUOUS_FILES),
        "load_failures": [],
    }
    args.out_json.parent.mkdir(parents=True, exist_ok=True)
    args.out_md.parent.mkdir(parents=True, exist_ok=True)
    args.out_json.write_text(
        json.dumps(report, ensure_ascii=False, indent=2) + "\n",
        encoding="utf-8",
    )
    args.out_md.write_text(render_markdown(report), encoding="utf-8")
    print(
        json.dumps(
            {
                "baseline": baseline["summary"]["overall"],
                "center_crop_60": cropped["summary"]["overall"],
                "background_shortcut_signal": baseline["background_shortcut"][
                    "signal_detected"
                ],
                "realwaste_framing_sensitive": report["analysis"]["crop_effect"][
                    "realwaste_framing_sensitive"
                ],
            },
            ensure_ascii=False,
            indent=2,
        )
    )
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (FileNotFoundError, RuntimeError, ValueError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        raise SystemExit(2)
