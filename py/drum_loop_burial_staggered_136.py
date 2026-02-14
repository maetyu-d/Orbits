import numpy as np


def _kick(n, sr):
    t = np.arange(n, dtype=np.float32) / float(sr)
    env = np.exp(-t / 0.15)
    freq = 53.0 * (1.0 + 3.2 * np.exp(-t * 8.2))
    return np.sin(2.0 * np.pi * freq * t) * env


def _snare(n, sr, decay=0.13):
    t = np.arange(n, dtype=np.float32) / float(sr)
    env = np.exp(-t / decay)
    noise = np.random.uniform(-1.0, 1.0, n).astype(np.float32)
    return (noise * 0.84 + 0.2 * np.sin(2.0 * np.pi * 210.0 * t)) * env


def _hat(n, sr):
    t = np.arange(n, dtype=np.float32) / float(sr)
    env = np.exp(-t / 0.02)
    noise = np.random.uniform(-1.0, 1.0, n).astype(np.float32)
    hp = noise - np.roll(noise, 1)
    return hp * env * 0.2


def generate(sr: int, duration: float, context=None):
    n = int(sr * duration)
    if n <= 0:
        return np.zeros((0,), dtype=np.float32)

    bpm = 136.0
    beat = 60.0 / bpm
    step = beat / 4.0
    steps = int(np.ceil(duration / step))

    y = np.zeros(n, dtype=np.float32)
    kick_steps = {0, 6, 9, 14}
    snare_steps = {4, 12}
    ghost_steps = {5, 13}
    hat_steps = {1, 2, 3, 7, 8, 10, 11, 15}

    for s in range(steps):
        swing = 0.14 * step if (s % 2 == 1) else 0.0
        start = int((s * step + swing) * sr)
        if start >= n:
            break

        if s in kick_steps:
            d = _kick(int(0.52 * sr), sr)
            end = min(n, start + len(d))
            y[start:end] += d[: end - start] * 0.9

        if s in snare_steps:
            d = _snare(int(0.32 * sr), sr)
            end = min(n, start + len(d))
            y[start:end] += d[: end - start] * 0.8

        if s in ghost_steps:
            d = _snare(int(0.16 * sr), sr, decay=0.08)
            end = min(n, start + len(d))
            y[start:end] += d[: end - start] * 0.28

        if s in hat_steps:
            d = _hat(int(0.08 * sr), sr)
            end = min(n, start + len(d))
            y[start:end] += d[: end - start]

    y += np.random.uniform(-1.0, 1.0, n).astype(np.float32) * 0.01
    y = np.tanh(y * 1.1)
    return np.stack([y, np.roll(y, 4)], axis=1).astype(np.float32)


if __name__ == "__main__":
    from _render_util import render_cli
    render_cli(generate)
