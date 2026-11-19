# Five-class model integration — 2026-11-15

## GitHub source

- Repository: `https://github.com/Bruiseunderskin/smart-trash-can`
- Fetched `origin/main`: `681edbc` (`docs(model): report unseen-card generalisation for the unknown class (#65)`)
- Model commit: `fe5d0be` (`feat(model): add a fifth unknown class for empty scenes (#64)`)
- Artifact: `models/wordless-unknown/wordless_s31.espdl`
- SHA-256: `cd8435624febc592a28053b20450a434b82a04c0642a70cb09c037e0b43910c3`
- Runtime CRC32: `0x401b7655`
- Labels: `food`, `hazardous`, `other`, `recyclable`, `unknown`
- Output: `[1,5]` softmax

`unknown` maps to `STC_BIN_COUNT`, forces `abstain = true`, opens no bin, and lets
the game retry the scan once. The first four model indices and bin mappings remain
unchanged: `{1,2,3,0}`.

## Diagnosis of the all-recyclable symptom

The firmware logs showed successful inference, not a classifier-startup fallback:

```text
LIVE_SCAN_FRAME ... class=3 conf=0.7..0.99 p=[...,...,...,recyclable]
```

Failed frames are discarded. Only successful frames enter the five-frame argmax,
so these results were real old-model predictions. The old firmware embedded the
four-class artifact `wordless_s31.espdl` with SHA-256
`233619c93f80888297a0a37c840fe422d70a779a4b421efe3d357dbf17a4ebd3`.

The model contract also warns that unknown card designs have substantially lower
generalisation than the known-card scene-split score. Real-camera framing and
lighting remain unmeasured.

## Local integration

- Build now embeds `models/wordless-unknown/wordless_s31.espdl`.
- CMake checks both SHA-256 and runtime CRC32.
- Vision buffers and aggregation use `STC_MODEL_CLASSES = 5`.
- `contracts/model/classes.json` is now v3.0.0.
- `contracts/model/preprocess.json` now declares output `[1,5]`.
- Replay self-test still uses the existing 40 four-class scenes.

## Target verification

- ESP-IDF 6.1 S31 build passed.
- Boot log:

```text
model_crc=401b7655 ... output_size=5
VISION_TASK=READY stack=8192 caps=PSRAM
```

- Replay self-test remained `39/40`; the sole error was `B-19_003`,
  `hazardous -> other`.
- The full app image was written in verified 1 MiB chunks after the host ran out
  of memory while `esptool` loaded the complete 8.5 MiB app image in one process.
