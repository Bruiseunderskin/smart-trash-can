# STC serial evidence

本目录保存 2026-11-05 在 `COM5` 上抓取的原始 UART 日志，不能被摘要代替。

- `serial-shell-ui.log`: 当前 GAME/CHAT shell 启动，记录两端触控对象建立、
  GAME 网络锁和契约版本。
- `serial-shell-fsm.log`: 当前外层状态机实测，含首次 CHAT、60 s 超时、
  再次 CHAT、停止网络并回 GAME。
- `serial-shell-status.log`: 回到 GAME 后的锁定快照：
  `app_state=0 voice_mode=0 network=0`。
- `serial-chat-debug-before.log`: 修复前 AFE 在模型加载前初始化、
  `detector=none` 的失败证据。
- `serial-chat-debug-after.log`: 中间证据，模型加载顺序已修复，但
  manual-stop 聆听仍未收到回答。
- `serial-chat-debug-final.log`: 中间证据，自动 VAD 已进入 `listening`；
  该次短采集未输入真人语音，不能单独作为对话通过证据。
- `serial-chat-debug-stats.log`: 修复后的真人语音证据，含麦克风输入/编码计数、
  VAD、`stt`、`tts` 和连续第二次回答。
- `serial-autonomous-5-rounds.log`: 历史自动五回合版本，仅作旧证据保留。
- `serial-cfg.log`: 历史自动五回合版本，仅作旧证据保留。
- `serial-chat-gate.log`: 历史 CHAT 互斥/网络停止证据。
- `serial-five-rounds-voice.log`: 历史十句本地 PCM 队列与打断证据。
- `serial-final-boot.log`: 历史自动五回合最终镜像启动证据。

日志保留原始时间戳和错误输出；`VALIDATION.md` 只解释当前 shell 已出现的事实。
历史日志中的固件哈希和游戏流程不能作为当前镜像的验收结果。
