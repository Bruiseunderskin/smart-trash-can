# Generated game assets

`game_replay_pack.bin` is generated from the 40 tracked test scenes with:

```sh
python3 tools/game_replay.py \
  --scene-dir data/mixed-card-object/wordless-v1/test-scenes \
  --output firmware/esp32s31/game/assets/game_replay_pack.bin \
  --seed 20261111
```

Expected SHA-256:

`287c8106c4be0569d15c06c1498d69f562af216030e2ccfe165545c9dd62722e`

The binary is tracked so ESP-IDF component dependency discovery remains
side-effect free. CMake rejects any replacement whose hash differs.

[Game guide](../README.md) · [Main component](../main/README.md)
