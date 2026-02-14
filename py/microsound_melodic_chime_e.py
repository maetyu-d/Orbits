import numpy as np


def generate(sr: int, duration: float, context=None):
    n = int(sr * duration)
    if n <= 0:
        return np.zeros((0,), dtype=np.float32)

    y = np.zeros(n, dtype=np.float32)
    root = 329.63  # E4
    steps = [0, 3, 7, 10, 12]

    for i, st in enumerate(steps):
        f = root * (2.0 ** (st / 12.0))
        start = int((0.0015 + i * 0.006) * sr)
        if start >= n:
            break
        length = min(int(0.045 * sr), n - start)
        t = np.arange(length, dtype=np.float32) / float(sr)
        env = np.exp(-t * (65.0 + i * 6.0))
        grain = np.sin(2.0 * np.pi * f * t)
        grain += 0.18 * np.sin(2.0 * np.pi * (f * 1.498) * t + 0.3)
        grain += 0.12 * np.sin(2.0 * np.pi * (f * 2.01) * t + 0.9)
        y[start:start + length] += grain * env * 0.17

    t_all = np.arange(n, dtype=np.float32) / float(sr)
    dust = (np.random.uniform(-1.0, 1.0, n).astype(np.float32) - np.roll(np.random.uniform(-1.0, 1.0, n).astype(np.float32), 1))
    y += dust * np.exp(-t_all * 160.0) * 0.02

    y = np.tanh(y * 1.65)
    return np.stack([np.roll(y, 1), y], axis=1).astype(np.float32)


if __name__ == "__main__":
    from _render_util import render_cli
    render_cli(generate)
