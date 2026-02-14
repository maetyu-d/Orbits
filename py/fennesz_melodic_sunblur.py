import numpy as np


def generate(sr: int, duration: float, context=None):
    n = int(sr * duration)
    if n <= 0:
        return np.zeros((0,), dtype=np.float32)

    t = np.arange(n, dtype=np.float32) / float(sr)
    y = np.zeros(n, dtype=np.float32)

    chord = [220.0, 277.18, 329.63, 415.30]
    for i, f in enumerate(chord):
        det = 1.0 + (i - 1.5) * 0.003
        wob = 1.0 + 0.002 * np.sin(2.0 * np.pi * (0.08 + 0.03 * i) * t)
        ph = 2.0 * np.pi * np.cumsum((f * det * wob) / float(sr)).astype(np.float32)
        y += (0.20 / (1.0 + i * 0.4)) * np.sin(ph)

    noise = np.random.default_rng(11).uniform(-1.0, 1.0, n).astype(np.float32) * 0.03
    env = 0.6 + 0.4 * np.sin(2.0 * np.pi * 0.12 * t + 0.2)
    y = (y + noise) * env
    y = np.tanh(y * 1.15).astype(np.float32)

    return np.stack([y, np.roll(y, 7)], axis=1)


if __name__ == "__main__":
    from _render_util import render_cli
    render_cli(generate)
