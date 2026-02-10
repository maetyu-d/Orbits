import numpy as np


def generate(sr: int, duration: float, context=None):
    n = int(sr * duration)
    if n <= 0:
        return np.zeros((0,), dtype=np.float32)

    t = np.arange(n, dtype=np.float32) / float(sr)

    env = np.exp(-t * 90.0)
    f0 = 2200.0
    chirp = f0 * (1.0 + 0.9 * np.exp(-t * 120.0))
    y = np.sin(2.0 * np.pi * chirp * t) * env

    partial = np.sin(2.0 * np.pi * 1.97 * chirp * t) * np.exp(-t * 130.0) * 0.45
    y = y + partial

    y *= 0.85
    y = np.clip(y, -1.0, 1.0)
    return np.stack([y, y], axis=1).astype(np.float32)


if __name__ == "__main__":
    from _render_util import render_cli
    render_cli(generate)
