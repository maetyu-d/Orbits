import numpy as np


def generate(sr: int, duration: float, context=None):
    n = int(sr * duration)
    if n <= 0:
        return np.zeros((0,), dtype=np.float32)

    t = np.arange(n, dtype=np.float32) / float(sr)
    f1, f2 = 540.0, 800.0
    y = (np.sin(2.0 * np.pi * f1 * t) + np.sin(2.0 * np.pi * f2 * t) * 0.9) * np.exp(-t * 24.0)
    y += np.sin(2.0 * np.pi * 2500.0 * t) * np.exp(-t * 95.0) * 0.1

    y = np.tanh(y * 1.45) * 0.7
    return np.stack([y, y], axis=1).astype(np.float32)


if __name__ == "__main__":
    from _render_util import render_cli
    render_cli(generate)
