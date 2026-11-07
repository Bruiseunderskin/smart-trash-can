"""B1 chat backend: PCM in -> JSON header + chunked PCM out.

STUB=1 runs with no upstream calls so the device side can be built today.
Provider functions are the only place vendor keys are used; they never reach the device.
"""
from __future__ import annotations

import asyncio, hashlib, io, json, os, struct, time
from pathlib import Path
from typing import AsyncIterator, Tuple

from fastapi import FastAPI, Request, UploadFile, File, Form, Header, HTTPException
from fastapi.responses import StreamingResponse

SR = 16000
ASSETS = Path(__file__).parent / "assets"
CACHE: dict[str, bytes] = {}
STUB = os.getenv("STUB", "1") == "1"
DEVICE_TOKEN = os.getenv("DEVICE_TOKEN", "")
ASR_TIMEOUT, LLM_TIMEOUT, TTS_TIMEOUT = 3.0, 5.0, 3.0

app = FastAPI(title="stc-chat")


def silence(ms: int = 500) -> bytes:
    return b"\x00\x00" * int(SR * ms / 1000)


def canned_wav_pcm(name: str = "fallback") -> bytes:
    p = ASSETS / f"{name}.pcm"
    return p.read_bytes() if p.is_file() else silence()


async def asr(pcm: bytes, lang: str) -> str:
    if STUB:
        return ""
    raise NotImplementedError("wire ASR_KEY provider here")


async def llm(text: str) -> str:
    if STUB:
        return "呢個係測試回覆。"
    raise NotImplementedError("wire LLM_KEY provider here")


async def tts_pcm(text: str) -> bytes:
    key = hashlib.sha1(text.encode()).hexdigest()[:16]
    if key in CACHE:
        return CACHE[key]
    data = canned_wav_pcm("reply") if STUB else b""
    if not data:
        raise NotImplementedError("wire TTS_KEY provider here")
    CACHE[key] = data
    return data


async def with_timeout(coro, seconds: float, default):
    try:
        return await asyncio.wait_for(coro, seconds)
    except (asyncio.TimeoutError, NotImplementedError):
        return default


def header_line(**kw) -> bytes:
    return (json.dumps(kw, ensure_ascii=False) + "\n").encode()


@app.get("/health")
async def health():
    return {"ok": True, "stub": STUB, "sr": SR}


@app.post("/ask")
async def ask(
    meta: str = Form(...),
    audio: UploadFile = File(...),
    x_device_token: str = Header(default=""),
):
    if DEVICE_TOKEN and x_device_token != DEVICE_TOKEN:
        raise HTTPException(status_code=401, detail="bad device token")

    try:
        m = json.loads(meta)
    except json.JSONDecodeError:
        raise HTTPException(status_code=400, detail="meta must be JSON")

    pcm = await audio.read()
    if len(pcm) < SR // 4:                      # < ~125 ms of audio
        raise HTTPException(status_code=400, detail="audio too short")

    started = time.monotonic()
    text = await with_timeout(asr(pcm, m.get("lang", "yue")), ASR_TIMEOUT, None)

    fallback = text is None
    if fallback:
        reply, out = "", canned_wav_pcm("fallback")
    else:
        reply = await with_timeout(llm(text), LLM_TIMEOUT, None)
        if reply is None:
            fallback, reply, out = True, "", canned_wav_pcm("fallback")
        else:
            out = await with_timeout(tts_pcm(reply), TTS_TIMEOUT, None)
            if out is None:
                fallback, out = True, canned_wav_pcm("fallback")

    ms = int((time.monotonic() - started) * 1000)

    async def body() -> AsyncIterator[bytes]:
        yield header_line(text=text or "", reply=reply or "", sr=SR,
                          fmt="s16le", ms=ms, fallback=int(fallback))
        for i in range(0, len(out), 4096):      # 4 KiB chunks ~ 128 ms of audio
            yield out[i : i + 4096]

    return StreamingResponse(body(), media_type="application/octet-stream")
