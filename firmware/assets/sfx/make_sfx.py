#!/usr/bin/env python3
"""Synthesise the game's short SFX set (original work, no third-party audio)."""
import math, wave, pathlib, subprocess
import numpy as np

SR = 48000
OUT = pathlib.Path(__file__).resolve().parent
OUT.mkdir(parents=True, exist_ok=True)

def tone(freq, dur, amp=0.6, decay=6.0, harm=0.0, attack=0.004):
    n = int(SR * dur)
    t = np.arange(n) / SR
    w = np.sin(2 * np.pi * freq * t)
    if harm:
        w = w + harm * np.sin(2 * np.pi * freq * 2 * t)
    env = np.exp(-decay * t)
    a = max(1, int(SR * attack))
    env[:a] *= np.linspace(0, 1, a)
    return amp * w * env

def silence(dur):
    return np.zeros(int(SR * dur))

def bell(freq, dur, amp=0.5, decay=5.0):
    n = int(SR * dur)
    t = np.arange(n) / SR
    w = (np.sin(2 * np.pi * freq * t)
         + 0.35 * np.sin(2 * np.pi * freq * 2 * t)
         + 0.12 * np.sin(2 * np.pi * freq * 3 * t))
    env = np.exp(-decay * t)
    a = max(1, int(SR * 0.003))
    env[:a] *= np.linspace(0, 1, a)
    return amp * w * env

def mix(*parts):
    n = max(len(p) for p in parts)
    out = np.zeros(n)
    for p in parts:
        out[:len(p)] += p
    return out

def place(buf, sig, at):
    i = int(SR * at)
    end = min(len(buf), i + len(sig))
    buf[i:end] += sig[:end - i]
    return buf

def write(name, sig):
    sig = np.clip(sig / max(1e-9, np.abs(sig).max()) * 0.85, -1, 1)
    data = (sig * 32767).astype(np.int16)
    wav = OUT / f"{name}.wav"
    with wave.open(str(wav), "wb") as f:
        f.setnchannels(1); f.setsampwidth(2); f.setframerate(SR)
        f.writeframes(data.tobytes())
    ogg = OUT / f"{name}.ogg"
    subprocess.run(["ffmpeg", "-y", "-loglevel", "error", "-i", str(wav),
                    "-ac", "1", "-ar", "48000", "-c:a", "libopus",
                    "-b:a", "48k", "-application", "audio", str(ogg)], check=True)
    wav.unlink()
    print(f"{name}: {len(sig)/SR:.2f}s  {ogg.stat().st_size/1024:.1f} KB")

# 1 掃描開始：單一短咇
write("sfx-scan", bell(1174.7, 0.16, amp=0.55, decay=14))

# 2 判斷完成：向上三音
b = silence(0.30)
place(b, bell(784.0, 0.14, 0.5, 12), 0.00)
place(b, bell(987.8, 0.14, 0.5, 12), 0.06)
place(b, bell(1174.7, 0.20, 0.55, 10), 0.12)
write("sfx-decide", b)

# 3 答對：亮琶音
b = silence(0.55)
for i, f in enumerate([1046.5, 1318.5, 1568.0, 2093.0]):
    place(b, bell(f, 0.34, 0.55 - i * 0.04, 7.5), i * 0.055)
write("sfx-correct", b)

# 4 答錯：低沉雙響，唔刺耳
b = silence(0.62)
place(b, tone(196.0, 0.26, 0.55, 7.0, harm=0.25), 0.00)
place(b, tone(174.6, 0.30, 0.5, 6.5, harm=0.2), 0.24)
write("sfx-wrong", b)

# 5 掃唔清：柔和中音兩下
b = silence(0.50)
place(b, bell(587.3, 0.16, 0.42, 10), 0.00)
place(b, bell(587.3, 0.20, 0.38, 9), 0.22)
write("sfx-retry", b)

# 6 五回合完：短號角和弦
b = silence(1.60)
for f, a in [(523.3, 0.34), (659.3, 0.30), (784.0, 0.28), (1046.5, 0.26)]:
    place(b, bell(f, 1.45, a, 2.2), 0.0)
place(b, bell(1568.0, 1.20, 0.22, 3.0), 0.10)
write("sfx-round", b)
