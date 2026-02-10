import sys
import wave
import struct
import inspect
import argparse
import tempfile
import subprocess
import shutil
import numpy as np
from pathlib import Path


def _normalize_audio(y: np.ndarray) -> np.ndarray:
    if y is None:
        return np.zeros((0,), dtype=np.float32)
    y = np.asarray(y, dtype=np.float32)
    if y.ndim == 2:
        # Accept shape (channels, samples) or (samples, channels)
        if y.shape[0] in (1, 2) and y.shape[1] > 2:
            y = y.T
    if y.ndim == 1:
        return y
    if y.ndim == 2:
        return y
    return y.reshape(-1)


def _to_int16(y: np.ndarray) -> np.ndarray:
    y = np.clip(y, -1.0, 1.0)
    return (y * 32767.0).astype(np.int16)


def write_wav(path: str, sr: int, y: np.ndarray):
    y = _normalize_audio(y)
    if y.ndim == 1:
        channels = 1
        frames = y
    else:
        channels = y.shape[1]
        frames = y

    with wave.open(path, "wb") as wf:
        wf.setnchannels(channels)
        wf.setsampwidth(2)
        wf.setframerate(int(sr))

        if channels == 1:
            data = _to_int16(frames)
            wf.writeframes(data.tobytes())
        else:
            data = _to_int16(frames)
            wf.writeframes(data.tobytes())


def _call_generate(generate_fn, sr: int, duration: float, context: dict):
    sig = inspect.signature(generate_fn)
    try:
        if len(sig.parameters) >= 3:
            return generate_fn(sr, duration, context)
        return generate_fn(sr, duration)
    except TypeError:
        return generate_fn(sr, duration)


def _default_trigger_output(script_name: str, track: int, when: float) -> Path:
    out_dir = Path(tempfile.gettempdir()) / "orbits_spiral_renders"
    out_dir.mkdir(parents=True, exist_ok=True)
    safe_when = f"{when:.3f}".replace(".", "_")
    return out_dir / f"{script_name}_track{track}_{safe_when}.wav"


def _play_file_async(path: str) -> bool:
    player = shutil.which("afplay")
    if not player and Path("/usr/bin/afplay").exists():
        player = "/usr/bin/afplay"
    if not player:
        return False
    try:
        subprocess.Popen([player, path], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        return True
    except Exception:
        return False


def render_cli(generate_fn):
    argv = sys.argv[1:]

    # Trigger mode used by the JUCE app:
    #   python script.py --track N --time T [--duration D] [--sample-rate SR]
    if argv and argv[0].startswith("--"):
        parser = argparse.ArgumentParser(add_help=False)
        parser.add_argument("--track", type=int, default=-1)
        parser.add_argument("--time", type=float, default=0.0)
        parser.add_argument("--duration", type=float, default=0.35)
        parser.add_argument("--sample-rate", type=int, default=48000)
        parser.add_argument("--output", type=str, default="")
        parser.add_argument("--no-play", action="store_true")
        args, _unknown = parser.parse_known_args(argv)

        script_stem = Path(sys.argv[0]).stem
        out_path = Path(args.output) if args.output else _default_trigger_output(script_stem, args.track, args.time)

        context = {
            "track": args.track,
            "time": args.time,
            "trigger": True,
        }

        y = _call_generate(generate_fn, int(args.sample_rate), float(args.duration), context)
        write_wav(str(out_path), int(args.sample_rate), y)

        if not args.no_play:
            played = _play_file_async(str(out_path))
            if not played:
                print("[orbits] warning: could not find/play via afplay")

        print(f"[orbits] rendered {out_path}")
        return

    # Offline render mode:
    #   python synth.py output.wav sample_rate duration_seconds
    if len(argv) < 3:
        print("Usage: python synth.py output.wav sample_rate duration_seconds")
        print("   or: python synth.py --track N --time T [--duration D] [--sample-rate SR]")
        sys.exit(1)

    out_path = argv[0]
    sr = int(float(argv[1]))
    duration = float(argv[2])
    y = _call_generate(generate_fn, sr, duration, {})
    write_wav(out_path, sr, y)


if __name__ == "__main__":
    print("This module provides render_cli().")
