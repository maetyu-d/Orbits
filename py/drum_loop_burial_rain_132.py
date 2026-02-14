import numpy as np


def _kick(n, sr):
    t = np.arange(n, dtype=np.float32) / float(sr)
    env = np.exp(-t / 0.18)
    sweep = np.exp(-t * 7.0)
    return np.sin(2.0 * np.pi * (49.0 * (1.0 + 3.1 * sweep)) * t) * env


def _snare(n, sr):
    t = np.arange(n, dtype=np.float32) / float(sr)
    env = np.exp(-t / 0.14)
    noise = np.random.uniform(-1.0, 1.0, n).astype(np.float32)
    tone = np.sin(2.0 * np.pi * 180.0 * t) * np.exp(-t * 20.0)
    return (noise * 0.75 + tone * 0.3) * env


def _rim(n, sr):
    t = np.arange(n, dtype=np.float32) / float(sr)
    env = np.exp(-t / 0.05)
    return np.sin(2.0 * np.pi * 1450.0 * t) * env * 0.5


def generate(sr: int, duration: float, context=None):
    n = int(sr * duration)
    if n <= 0:
        return np.zeros((0,), dtype=np.float32)

    bpm = 132.0
    beat = 60.0 / bpm
    step = beat / 4.0
    steps = int(np.ceil(duration / step))

    y = np.zeros(n, dtype=np.float32)
    kick_steps = {0, 6, 10}
    snare_steps = {4, 12}
    rim_steps = {3, 7, 11, 15}

    for s in range(steps):
        swing = 0.12 * step if (s % 2 == 1) else 0.0
        start = int((s * step + swing) * sr)
        if start >= n:
            break

        if s in kick_steps:
            d = _kick(int(0.62 * sr), sr)
            end = min(n, start + len(d))
            y[start:end] += d[: end - start] * 0.92

        if s in snare_steps:
            d = _snare(int(0.32 * sr), sr)
            end = min(n, start + len(d))
            y[start:end] += d[: end - start] * 0.78

        if s in rim_steps:
            d = _rim(int(0.09 * sr), sr)
            end = min(n, start + len(d))
            y[start:end] += d[: end - start] * 0.26

    t = np.arange(n, dtype=np.float32) / float(sr)
    rain = np.random.uniform(-1.0, 1.0, n).astype(np.float32)
    rain = (rain - np.roll(rain, 1)) * np.exp(-t * 8.0) * 0.01
    bed = np.random.uniform(-1.0, 1.0, n).astype(np.float32) * 0.015
    y = np.tanh((y + bed + rain) * 1.06)
    return np.stack([np.roll(y, 4), y], axis=1).astype(np.float32)


if __name__ == "__main__":
    from _render_util import render_cli
    render_cli(generate)
