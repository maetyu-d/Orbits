import numpy as np


def _kick(n, sr):
    t = np.arange(n, dtype=np.float32) / float(sr)
    env = np.exp(-t / 0.2)
    return np.sin(2.0 * np.pi * (48.0 * (1.0 + 3.0 * np.exp(-t * 7.5))) * t) * env


def _clap_snare(n, sr):
    t = np.arange(n, dtype=np.float32) / float(sr)
    noise = np.random.uniform(-1.0, 1.0, n).astype(np.float32)
    env = np.exp(-t / 0.13)
    bursts = np.zeros(n, dtype=np.float32)
    for off in [0, int(0.009 * sr), int(0.017 * sr)]:
        if off < n:
            L = n - off
            bursts[off:] += np.exp(-np.arange(L, dtype=np.float32) / (0.045 * sr))
    return (noise * 0.7 * bursts + 0.22 * np.sin(2.0 * np.pi * 190.0 * t)) * env


def _tick(n, sr):
    t = np.arange(n, dtype=np.float32) / float(sr)
    return np.sin(2.0 * np.pi * 2300.0 * t) * np.exp(-t / 0.02) * 0.22


def generate(sr: int, duration: float, context=None):
    n = int(sr * duration)
    if n <= 0:
        return np.zeros((0,), dtype=np.float32)

    bpm = 130.0
    beat = 60.0 / bpm
    step = beat / 4.0
    steps = int(np.ceil(duration / step))

    y = np.zeros(n, dtype=np.float32)
    kick_steps = {0, 7, 10}
    snare_steps = {4, 12}
    tick_steps = {2, 3, 6, 8, 11, 13, 15}

    for s in range(steps):
        swing = 0.1 * step if (s % 2 == 1) else 0.0
        start = int((s * step + swing) * sr)
        if start >= n:
            break

        if s in kick_steps:
            d = _kick(int(0.65 * sr), sr)
            end = min(n, start + len(d))
            y[start:end] += d[: end - start] * 0.9

        if s in snare_steps:
            d = _clap_snare(int(0.34 * sr), sr)
            end = min(n, start + len(d))
            y[start:end] += d[: end - start] * 0.75

        if s in tick_steps:
            d = _tick(int(0.08 * sr), sr)
            end = min(n, start + len(d))
            y[start:end] += d[: end - start] * 0.5

    bed = np.random.uniform(-1.0, 1.0, n).astype(np.float32) * 0.012
    y = np.tanh((y + bed) * 1.07)
    return np.stack([np.roll(y, 6), y], axis=1).astype(np.float32)


if __name__ == "__main__":
    from _render_util import render_cli
    render_cli(generate)
