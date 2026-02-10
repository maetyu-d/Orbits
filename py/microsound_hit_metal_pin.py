import numpy as np


def generate(sr: int, duration: float, context=None):
    n = int(sr * duration)
    if n <= 0:
        return np.zeros((0,), dtype=np.float32)

    t = np.arange(n, dtype=np.float32) / float(sr)

    freqs = np.array([1180.0, 1730.0, 2470.0, 3310.0], dtype=np.float32)
    decs = np.array([38.0, 50.0, 65.0, 88.0], dtype=np.float32)

    y = np.zeros(n, dtype=np.float32)
    for f, d in zip(freqs, decs):
        y += np.sin(2.0 * np.pi * f * t) * np.exp(-t * d)

    transient = np.random.uniform(-1.0, 1.0, n).astype(np.float32) * np.exp(-t * 220.0) * 0.2
    y = (y * 0.22) + transient

    y = np.tanh(y * 1.5)
    return np.stack([y, y], axis=1).astype(np.float32)


if __name__ == "__main__":
    from _render_util import render_cli
    render_cli(generate)
