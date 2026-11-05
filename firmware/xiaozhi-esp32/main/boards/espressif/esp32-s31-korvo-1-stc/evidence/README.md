# P1 serial evidence

本目录保存 2026-11-05 在 `COM5` 上抓取的原始 UART 日志，不能被摘要代替。

- `serial-autonomous-5-rounds.log`: 早期五回合冒烟日志；用于确认首版完整流程。
- `serial-cfg.log`: 最终行为验收日志，包含 GAME 零云端连接、模式互斥、
  CHAT 进入/60 s 退出、第二次五回合及再次进入 CHAT。
- `serial-chat-gate.log`: CHAT 中拒绝新局、退出 CHAT、停止网络并重启 GAME。
- `serial-five-rounds-voice.log`: 十句本地 PCM 顺序播放、队列满与主动打断。
- `serial-final-boot.log`: 最终镜像启动、四份契约版本、GAME 网络闸与首句时间。

日志保留原始时间戳和错误输出；`VALIDATION.md` 只解释已出现的事实。早期日志
中的固件哈希与最终构建不同，不能作为最终镜像哈希使用。
