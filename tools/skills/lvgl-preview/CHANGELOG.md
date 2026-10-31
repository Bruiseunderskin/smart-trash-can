# Changelog

## 2026-11-01

- Replaced the mandatory human decision gate with native render/evidence evaluation.
- Added state/variant coverage, stale/blank capture detection, native check reports and hash-bound visual inspection.
- Added PASS/FAIL/INSUFFICIENT_EVIDENCE results and run replacement without persisted-PID termination.
- Fixed standalone skill validation and repeated display-call semicolons.
- Retained exact backup restoration and optional explicit human feedback.

## 2026-09-05

- Added a deterministic framebuffer screenshot workflow for native SDL previews.
- Documented the macOS BMP-to-PNG conversion path and the software-renderer retry for black accelerated readbacks.
- Recorded that screenshot evidence does not replace the explicit `OK` / `NO-GO` preview decision or physical display verification.
