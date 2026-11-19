#!/usr/bin/env python3
"""Capture the S3 TOF detector until it latches an object-drop event."""

from __future__ import annotations

import argparse
import time
from pathlib import Path


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", default="COM9")
    parser.add_argument("--baudrate", type=int, default=115200)
    parser.add_argument("--timeout", type=float, default=600.0)
    parser.add_argument("--log", type=Path, required=True)
    parser.add_argument("--reset", action="store_true")
    args = parser.parse_args()

    import serial

    args.log.parent.mkdir(parents=True, exist_ok=True)
    with serial.Serial(args.port, args.baudrate, timeout=0.2) as port:
        port.dtr = False
        port.rts = False
        if args.reset:
            port.rts = True
            time.sleep(0.15)
            port.rts = False
        deadline = time.monotonic() + args.timeout
        with args.log.open("w", encoding="utf-8", errors="replace", newline="") as log:
            while time.monotonic() < deadline:
                raw = port.readline()
                if not raw:
                    continue
                line = raw.decode("utf-8", errors="replace").rstrip("\r\n")
                log.write(line + "\n")
                log.flush()
                if "OBJECT_DETECTOR=STOPPED" in line:
                    return 0
    return 2


if __name__ == "__main__":
    raise SystemExit(main())
