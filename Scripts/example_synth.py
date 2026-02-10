#!/usr/bin/env python3
"""Example script triggered by Spiral Synth Timeline.

The app invokes this as:
    python3 script.py --track <index> --time <seconds>

Replace the body with your own synthesis engine (e.g. pyo, sounddevice, csound, custom DSP).
"""

import argparse
import math
import struct
import tempfile
import wave
from pathlib import Path


def render_kick(path: Path, sample_rate: int = 48000) -> None:
    duration = 0.22
    num_samples = int(duration * sample_rate)

    with wave.open(str(path), "wb") as wf:
        wf.setnchannels(1)
        wf.setsampwidth(2)
        wf.setframerate(sample_rate)

        for i in range(num_samples):
            t = i / sample_rate
            freq = 160.0 * math.exp(-t * 24.0) + 40.0
            env = math.exp(-t * 18.0)
            sample = math.sin(2.0 * math.pi * freq * t) * env
            sample = max(-1.0, min(1.0, sample))
            wf.writeframes(struct.pack("<h", int(sample * 32767.0)))


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--track", type=int, default=-1)
    parser.add_argument("--time", type=float, default=0.0)
    args = parser.parse_args()

    out_file = Path(tempfile.gettempdir()) / f"spiral_track_{args.track}_trigger.wav"
    render_kick(out_file)

    print(f"[spiral] trigger track={args.track} t={args.time:.3f}s -> {out_file}")


if __name__ == "__main__":
    main()
