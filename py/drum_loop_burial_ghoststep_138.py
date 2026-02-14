import numpy as np


def _kick(n, sr, freq=52.0, decay=0.16):
    t = np.arange(n, dtype=np.float32) / float(sr)
    env = np.exp(-t / decay)
    pitch = np.exp(-t * 8.5)
    body = np.sin(2.0 * np.pi * (freq * (1.0 + 3.6 * pitch)) * t)
    click = np.sin(2.0 * np.pi * 1900.0 * t) * np.exp(-t * 95.0)
    return body * env * 0.95 + click * 0.08


def _snare(n, sr, decay=0.15):
    t = np.arange(n, dtype=np.float32) / float(sr)
    env = np.exp(-t / decay)
    noise = np.random.uniform(-1.0, 1.0, n).astype(np.float32)
    tone = np.sin(2.0 * np.pi * 205.0 * t) * np.exp(-t * 18.0)
    return (noise * 0.82 + tone * 0.25) * env


def _hat(n, sr, decay=0.03):
    t = np.arange(n, dtype=np.float32) / float(sr)
    env = np.exp(-t / decay)
    noise = np.random.uniform(-1.0, 1.0, n).astype(np.float32)
    hp = noise - np.roll(noise, 1)
    return hp * env * 0.22


def generate(sr: int, duration: float, context=None):
    n = int(sr * duration)
    if n <= 0:
        return np.zeros((0,), dtype=np.float32)

    bpm = 138.0
    beat = 60.0 / bpm
    step = beat / 4.0
    steps = int(np.ceil(duration / step))

    y = np.zeros(n, dtype=np.float32)
    kick_steps = {0, 5, 11}
    snare_steps = {4, 12}
    ghost_snare_steps = {7, 15}
    hat_steps = {1, 3, 6, 9, 10, 13, 14}

    for s in range(steps):
        swing = 0.18 * step if (s % 2 == 1) else 0.0
        start = int((s * step + swing) * sr)
        if start >= n:
            break

        if s in kick_steps:
            k = _kick(int(0.55 * sr), sr)
            end = min(n, start + len(k))
            y[start:end] += k[: end - start] * 0.95

        if s in snare_steps:
            sn = _snare(int(0.36 * sr), sr)
            end = min(n, start + len(sn))
            y[start:end] += sn[: end - start] * 0.82

        if s in ghost_snare_steps:
            gs = _snare(int(0.18 * sr), sr)
            end = min(n, start + len(gs))
            y[start:end] += gs[: end - start] * 0.24

        if s in hat_steps:
            h = _hat(int(0.12 * sr), sr)
            end = min(n, start + len(h))
            y[start:end] += h[: end - start]

    t = np.arange(n, dtype=np.float32) / float(sr)
    hiss = np.random.uniform(-1.0, 1.0, n).astype(np.float32) * 0.012 * (0.6 + 0.4 * np.sin(2.0 * np.pi * 0.09 * t))
    y = np.tanh((y + hiss) * 1.08)
    return np.stack([y, np.roll(y, 8)], axis=1).astype(np.float32)


if __name__ == "__main__":
    from _render_util import render_cli
    render_cli(generate)
