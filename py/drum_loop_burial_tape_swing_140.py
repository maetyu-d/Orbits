import numpy as np


def _kick(n, sr):
    t = np.arange(n, dtype=np.float32) / float(sr)
    env = np.exp(-t / 0.14)
    f = 55.0 * (1.0 + 2.8 * np.exp(-t * 8.0))
    return np.sin(2.0 * np.pi * f * t) * env


def _snare(n, sr):
    t = np.arange(n, dtype=np.float32) / float(sr)
    noise = np.random.uniform(-1.0, 1.0, n).astype(np.float32)
    env = np.exp(-t / 0.12)
    return (noise * 0.85 + 0.22 * np.sin(2.0 * np.pi * 210.0 * t)) * env


def _hat(n, sr, amp=0.2):
    t = np.arange(n, dtype=np.float32) / float(sr)
    env = np.exp(-t / 0.025)
    noise = np.random.uniform(-1.0, 1.0, n).astype(np.float32)
    hp = noise - np.roll(noise, 1)
    return hp * env * amp


def generate(sr: int, duration: float, context=None):
    n = int(sr * duration)
    if n <= 0:
        return np.zeros((0,), dtype=np.float32)

    bpm = 140.0
    beat = 60.0 / bpm
    step = beat / 4.0
    steps = int(np.ceil(duration / step))

    y = np.zeros(n, dtype=np.float32)
    kick_steps = {0, 3, 9, 11}
    snare_steps = {4, 12}
    hat_steps = {1, 2, 5, 6, 8, 10, 13, 14, 15}

    for s in range(steps):
        swing = 0.16 * step if (s % 2 == 1) else 0.0
        start = int((s * step + swing) * sr)
        if start >= n:
            break

        if s in kick_steps:
            d = _kick(int(0.48 * sr), sr)
            end = min(n, start + len(d))
            y[start:end] += d[: end - start] * 0.92

        if s in snare_steps:
            d = _snare(int(0.28 * sr), sr)
            end = min(n, start + len(d))
            y[start:end] += d[: end - start] * 0.76

        if s in hat_steps:
            amp = 0.12 if s in {2, 6, 10, 14} else 0.19
            d = _hat(int(0.1 * sr), sr, amp=amp)
            end = min(n, start + len(d))
            y[start:end] += d[: end - start]

    wow = 1.0 + 0.01 * np.sin(2.0 * np.pi * 0.5 * np.arange(n, dtype=np.float32) / float(sr))
    y = np.tanh(y * wow * 1.08)
    return np.stack([y, np.roll(y, 5)], axis=1).astype(np.float32)


if __name__ == "__main__":
    from _render_util import render_cli
    render_cli(generate)
