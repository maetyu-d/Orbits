import numpy as np


def _kick(n, sr):
    t = np.arange(n, dtype=np.float32) / float(sr)
    env = np.exp(-t / 0.14)
    f = 56.0 * (1.0 + 3.5 * np.exp(-t * 9.0))
    return np.sin(2.0 * np.pi * f * t) * env


def _snare(n, sr):
    t = np.arange(n, dtype=np.float32) / float(sr)
    env = np.exp(-t / 0.11)
    noise = np.random.uniform(-1.0, 1.0, n).astype(np.float32)
    tone = np.sin(2.0 * np.pi * 220.0 * t) * np.exp(-t * 30.0)
    return (noise * 0.86 + tone * 0.18) * env


def _shaker(n, sr):
    t = np.arange(n, dtype=np.float32) / float(sr)
    env = np.exp(-t / 0.04)
    noise = np.random.uniform(-1.0, 1.0, n).astype(np.float32)
    return (noise - np.roll(noise, 1)) * env * 0.16


def generate(sr: int, duration: float, context=None):
    n = int(sr * duration)
    if n <= 0:
        return np.zeros((0,), dtype=np.float32)

    bpm = 142.0
    beat = 60.0 / bpm
    step = beat / 4.0
    steps = int(np.ceil(duration / step))

    y = np.zeros(n, dtype=np.float32)
    kick_steps = {0, 5, 8, 11}
    snare_steps = {4, 12}
    shaker_steps = {1, 2, 3, 6, 7, 9, 10, 13, 14, 15}

    for s in range(steps):
        swing = 0.13 * step if (s % 2 == 1) else 0.0
        start = int((s * step + swing) * sr)
        if start >= n:
            break

        if s in kick_steps:
            d = _kick(int(0.45 * sr), sr)
            end = min(n, start + len(d))
            y[start:end] += d[: end - start] * 0.9

        if s in snare_steps:
            d = _snare(int(0.25 * sr), sr)
            end = min(n, start + len(d))
            y[start:end] += d[: end - start] * 0.79

        if s in shaker_steps:
            d = _shaker(int(0.09 * sr), sr)
            end = min(n, start + len(d))
            y[start:end] += d[: end - start]

    t = np.arange(n, dtype=np.float32) / float(sr)
    duck = 0.9 + 0.1 * np.sin(2.0 * np.pi * 0.08 * t)
    y = np.tanh(y * duck * 1.12)
    return np.stack([np.roll(y, 3), y], axis=1).astype(np.float32)


if __name__ == "__main__":
    from _render_util import render_cli
    render_cli(generate)
