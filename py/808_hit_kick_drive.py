import numpy as np


def generate(sr: int, duration: float, context=None):
    n = int(sr * duration)
    if n <= 0:
        return np.zeros((0,), dtype=np.float32)

    t = np.arange(n, dtype=np.float32) / float(sr)
    f = 44.0 + 140.0 * np.exp(-t * 18.0)
    body = np.sin(2.0 * np.pi * f * t)
    env = np.exp(-t * 9.0)
    click = np.sin(2.0 * np.pi * 2400.0 * t) * np.exp(-t * 120.0) * 0.12

    y = (body * env * 1.2) + click
    y = np.tanh(y * 1.8) * 0.78
    return np.stack([y, y], axis=1).astype(np.float32)


if __name__ == "__main__":
    from _render_util import render_cli
    render_cli(generate)
