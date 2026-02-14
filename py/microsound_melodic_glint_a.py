import numpy as np


def generate(sr: int, duration: float, context=None):
    n = int(sr * duration)
    if n <= 0:
        return np.zeros((0,), dtype=np.float32)

    y = np.zeros(n, dtype=np.float32)
    rng = np.random.default_rng()

    base = 440.0  # A4
    ratios = [1.0, 5.0 / 4.0, 3.0 / 2.0, 2.0]

    for i in range(8):
        start = int((0.003 + i * 0.0045) * sr)
        if start >= n:
            break
        length = min(int(0.03 * sr), n - start)
        t = np.arange(length, dtype=np.float32) / float(sr)
        f = base * ratios[i % len(ratios)]
        env = np.exp(-t * (70.0 + i * 8.0))
        grain = np.sin(2.0 * np.pi * f * t + rng.uniform(0.0, 1.0))
        grain += 0.25 * np.sin(2.0 * np.pi * (f * 2.01) * t)
        y[start:start + length] += grain * env * 0.16

    t_all = np.arange(n, dtype=np.float32) / float(sr)
    y += np.random.uniform(-1.0, 1.0, n).astype(np.float32) * np.exp(-t_all * 120.0) * 0.03
    y = np.tanh(y * 1.7)
    return np.stack([y, np.roll(y, 2)], axis=1).astype(np.float32)


if __name__ == "__main__":
    from _render_util import render_cli
    render_cli(generate)
