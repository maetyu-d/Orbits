import numpy as np


def generate(sr: int, duration: float, context=None):
    n = int(sr * duration)
    if n <= 0:
        return np.zeros((0,), dtype=np.float32)

    t = np.arange(n, dtype=np.float32) / float(sr)

    f_start = 4200.0
    f_end = 820.0
    f = f_end + (f_start - f_end) * np.exp(-t * 70.0)

    tone = np.sin(2.0 * np.pi * f * t)
    wow = np.sin(2.0 * np.pi * 7.5 * t) * 0.012
    tone2 = np.sin(2.0 * np.pi * (f * (1.0 + wow)) * t)

    env = np.exp(-t * 54.0)
    y = (0.65 * tone + 0.35 * tone2) * env
    y += np.random.uniform(-1.0, 1.0, n).astype(np.float32) * np.exp(-t * 140.0) * 0.09

    y = np.tanh(y * 1.35)
    return np.stack([y, y], axis=1).astype(np.float32)


if __name__ == "__main__":
    from _render_util import render_cli
    render_cli(generate)
