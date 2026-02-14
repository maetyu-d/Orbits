import numpy as np


def generate(sr: int, duration: float, context=None):
    n = int(sr * duration)
    if n <= 0:
        return np.zeros((0,), dtype=np.float32)

    t = np.arange(n, dtype=np.float32) / float(sr)
    y = np.zeros(n, dtype=np.float32)

    freqs = [146.83, 220.0, 293.66, 369.99]
    for i, f in enumerate(freqs):
        am = 0.7 + 0.3 * np.sin(2.0 * np.pi * (0.11 + 0.02 * i) * t + i * 0.5)
        fm = 1.0 + 0.003 * np.sin(2.0 * np.pi * (3.7 + 0.4 * i) * t)
        ph = 2.0 * np.pi * np.cumsum((f * fm) / float(sr)).astype(np.float32)
        voice = np.sin(ph) + 0.15 * np.sin(2.0 * ph + 0.6)
        y += voice * am * (0.11 / (1.0 + i * 0.25))

    noise = np.random.default_rng(58).uniform(-1.0, 1.0, n).astype(np.float32) * 0.017
    y += noise
    y = np.tanh(y * 1.22).astype(np.float32)

    return np.stack([y, y], axis=1)


if __name__ == "__main__":
    from _render_util import render_cli
    render_cli(generate)
