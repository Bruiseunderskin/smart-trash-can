#!/usr/bin/env python3
"""Drive the P1 serial acceptance scenarios and preserve their complete log."""
from pathlib import Path
import argparse
import time

import serial


MARKERS = (
    "STC_NETWORK_GATE",
    "STC_CMD",
    "GAME_START",
    "STUB_LABEL",
    "STUB_DROP",
    "GAME_SETTLEMENT",
    "CONTRACT_VERSION",
    "CHAT idle timeout",
    "State: activating -> idle",
    "State: idle -> listening",
)


def send(port: serial.Serial, command: str) -> None:
    port.write(f"{command}\n".encode())
    port.flush()


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", default="COM5")
    parser.add_argument("--seconds", type=float, default=150)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()

    args.output.parent.mkdir(parents=True, exist_ok=True)
    port = serial.Serial(args.port, 115200, timeout=0.5)
    port.dtr = False
    port.rts = True
    time.sleep(0.15)
    port.rts = False

    started = time.monotonic()
    deadline = started + args.seconds
    reject_sent = False
    first_chat = False
    second_game = False
    second_chat = False
    second_chat_idle = False
    settlement_count = 0

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

            if not reject_sent and "GAME_START result=0" in text:
                send(port, "stc chat")
                reject_sent = True

            if "GAME_SETTLEMENT rounds=5" in text:
                settlement_count += 1
                if settlement_count == 1 and not first_chat:
                    send(port, "stc chat")
                    first_chat = True
                elif settlement_count == 2 and not second_chat:
                    send(port, "stc chat")
                    second_chat = True

            if "CHAT idle timeout" in text and first_chat and not second_game:
                send(port, "stc game")
                second_game = True

            if (second_chat and "State: connecting -> listening" in text
                    and not second_chat_idle):
                send(port, "stc play")
                time.sleep(0.5)
                send(port, "stc game")
                second_chat_idle = True
                break

    port.close()
    print(f"CAPTURE seconds={time.monotonic() - started:.1f} "
          f"settlements={settlement_count}", flush=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
