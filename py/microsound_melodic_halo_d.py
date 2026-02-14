import numpy as np


def generate(sr: int, duration: float, context=None):
    n = int(sr * duration)
    if n <= 0:
        return np.zeros((0,), dtype=np.float32)

    y = np.zeros(n, dtype=np.float32)
    root = 293.66  # D4

    # Upward pointillist pattern.
    for i in range(10):
        st = [0, 2, 5, 7, 10][i % 5]
        f = root * (2.0 ** (st / 12.0))
        start = int((0.001 + i * 0.0038) * sr)
        if start >= n:
            break
        length = min(int(0.03 * sr), n - start)
        t = np.arange(length, dtype=np.float32) / float(sr)
        env = np.exp(-t * (85.0 + 5.0 * i))
        tone = np.sin(2.0 * np.pi * f * t + i * 0.12)
        y[start:start + length] += tone * env * 0.14

    t_all = np.arange(n, dtype=np.float32) / float(sr)
    halo = 0.05 * np.sin(2.0 * np.pi * 1180.0 * t_all) * np.exp(-t_all * 95.0)
    y = np.tanh((y + halo) * 1.7)
    return np.stack([y, y], axis=1).astype(np.float32)


if __name__ == "__main__":
    from _render_util import render_cli
    render_cli(generate)
