import numpy as np


def generate(sr: int, duration: float, context=None):
    n = int(sr * duration)
    if n <= 0:
        return np.zeros((0,), dtype=np.float32)

    t = np.arange(n, dtype=np.float32) / float(sr)
    f = 90.0 + 70.0 * np.exp(-t * 14.0)
    tone = np.sin(2.0 * np.pi * f * t)
    overtone = np.sin(2.0 * np.pi * (f * 2.0) * t) * 0.22
    env = np.exp(-t * 12.0)

    y = (tone + overtone) * env
    y = np.tanh(y * 1.5) * 0.74
    return np.stack([y, y], axis=1).astype(np.float32)


if __name__ == "__main__":
    from _render_util import render_cli
    render_cli(generate)
