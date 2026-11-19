# S31 game main component

Stage-one standalone game firmware:

- Korvo-1 BSP display/touch and ES8389 speaker.
- ESP-DL wordless-padded int8 model.
- Deterministic replay pack built from the 40 test scenes.
- ESP-NOW controller using `stc_hand_msg`.
- Shared game core/controller/UI contracts.

There is no camera, servo, or VL6180X driver in this component.

[Game build guide](../README.md) · [Firmware index](../../../README.md)
