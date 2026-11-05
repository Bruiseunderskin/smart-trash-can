#!/usr/bin/env python3
"""Generate 16 kHz mono PCM firmware assets from WAV files or Edge TTS."""
from pathlib import Path
import argparse
import json
import shutil
import subprocess
import tempfile
import wave


def resolve_ffmpeg(explicit_path: Path | None) -> str:
    if explicit_path is not None:
        if not explicit_path.is_file():
            raise FileNotFoundError(f"missing ffmpeg: {explicit_path}")
        return str(explicit_path)
    discovered = shutil.which("ffmpeg")
    if discovered:
        return discovered
    try:
        import imageio_ffmpeg
    except ImportError as error:
        raise RuntimeError(
            "ffmpeg not found; install imageio-ffmpeg or pass --ffmpeg"
        ) from error
    return imageio_ffmpeg.get_ffmpeg_exe()


def synthesize_wav(edge_tts: str, ffmpeg: str, voice: str, text: str,
                   destination: Path, work_dir: Path) -> None:
    mp3 = work_dir / f"{destination.stem}.mp3"
    subprocess.run(
        [
            edge_tts,
            "--voice",
            voice,
            "--text",
            text,
            "--write-media",
            str(mp3),
        ],
        check=True,
    )
    subprocess.run(
        [
            ffmpeg,
            "-hide_banner",
            "-loglevel",
            "error",
            "-y",
            "-i",
            str(mp3),
            "-ac",
            "1",
            "-ar",
            "16000",
            "-sample_fmt",
            "s16",
            str(destination),
        ],
        check=True,
    )


def read_validated_wav(source: Path) -> bytes:
    with wave.open(str(source), "rb") as wav:
        if (wav.getnchannels() != 1 or wav.getframerate() != 16000
                or wav.getsampwidth() != 2):
            raise ValueError(
                f"{source}: expected mono / 16000 Hz / 16-bit PCM")
        return wav.readframes(wav.getnframes())


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--phrases", required=True, type=Path)
    parser.add_argument("--output-dir", required=True, type=Path)
    source = parser.add_mutually_exclusive_group(required=True)
    source.add_argument("--source-dir", type=Path,
                        help="directory containing <phrase-key>.wav")
    source.add_argument("--tts-voice",
                        help="Edge TTS voice, e.g. zh-HK-HiuMaanNeural")
    parser.add_argument("--edge-tts",
                        help="Edge TTS executable; defaults to PATH")
    parser.add_argument("--ffmpeg",
                        type=Path,
                        help="ffmpeg executable; defaults to PATH/imageio")
    args = parser.parse_args()

    phrases = json.loads(args.phrases.read_text(encoding="utf-8"))
    args.output_dir.mkdir(parents=True, exist_ok=True)
    edge_tts = None
    ffmpeg = None
    if args.tts_voice:
        edge_tts = args.edge_tts or shutil.which("edge-tts")
        if not edge_tts:
            raise RuntimeError(
                "Edge TTS executable not found; install edge-tts or pass "
                "--edge-tts")
        ffmpeg = resolve_ffmpeg(args.ffmpeg)

    with tempfile.TemporaryDirectory(prefix="stc-voice-") as temp_name:
        work_dir = Path(temp_name)
        for key, spec in phrases.items():
            if args.source_dir:
                wav_source = args.source_dir / f"{key}.wav"
                if not wav_source.is_file():
                    raise FileNotFoundError(f"missing recording: {wav_source}")
            else:
                wav_source = work_dir / f"{key}.wav"
                synthesize_wav(
                    edge_tts, ffmpeg, args.tts_voice, spec["text"],
                    wav_source, work_dir)
            pcm = read_validated_wav(wav_source)
            if not pcm:
                raise ValueError(f"{wav_source}: generated an empty asset")
            target = args.output_dir / spec["pcm"]
            target.write_bytes(pcm)
            print(f"{key}: {wav_source} -> {target} ({len(pcm)} bytes)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
