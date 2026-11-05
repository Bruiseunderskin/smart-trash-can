#!/usr/bin/env python3
"""Drive the GAME/CHAT shell over UART and preserve the complete serial log."""
from pathlib import Path
import argparse
import time

import serial


MARKERS = (
    "STC_SHELL_UI",
    "STC_NETWORK_GATE",
    "STC_MODE_REQUEST",
    "APP_STATE",
    "CONTRACT_VERSION",
    "CHAT idle timeout",
    "State: connecting -> listening",
    "State: listening -> idle",
)


def send(port: serial.Serial, command: str) -> None:
    port.write(f"{command}\n".encode())
    port.flush()


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", default="COM5")
    parser.add_argument("--seconds", type=float, default=110)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument(
        "--skip-timeout",
        action="store_true",
        help="return to GAME immediately after the first CHAT becomes ready",
    )
    args = parser.parse_args()

    args.output.parent.mkdir(parents=True, exist_ok=True)
    port = serial.Serial(args.port, 115200, timeout=0.3)
    port.dtr = False
    port.rts = True
    time.sleep(0.15)
    port.rts = False

    started = time.monotonic()
    deadline = started + args.seconds
    stage = 0
    with args.output.open("wb") as log:
        while time.monotonic() < deadline:
            line = port.readline()
            if not line:
                continue
            log.write(line)
            log.flush()
            text = line.decode("utf-8", "replace").strip()
            if any(marker in text for marker in MARKERS):
                print(text, flush=True)

            if stage == 0 and "STC_SHELL_UI ready" in text:
                send(port, "stc chat")
                stage = 1
            elif stage == 1 and "APP_STATE CHAT_STARTING -> CHAT_READY" in text:
                if args.skip_timeout:
                    send(port, "stc game")
                    stage = 4
                else:
                    stage = 2
            elif stage == 2 and "APP_STATE CHAT_TIMEOUT -> GAME" in text:
                send(port, "stc chat")
                stage = 3
            elif stage == 3 and "APP_STATE CHAT_STARTING -> CHAT_READY" in text:
                send(port, "stc game")
                stage = 4
            elif stage == 4 and "APP_STATE CHAT -> GAME" in text:
                stage = 5
                break

    port.close()
    print(
        f"CAPTURE stage={stage} seconds={time.monotonic() - started:.1f}",
        flush=True,
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
