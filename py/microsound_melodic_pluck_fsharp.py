import numpy as np


def generate(sr: int, duration: float, context=None):
    n = int(sr * duration)
    if n <= 0:
        return np.zeros((0,), dtype=np.float32)

    y = np.zeros(n, dtype=np.float32)
    f0 = 369.99  # F#4

    for i, mult in enumerate([1.0, 1.12246, 1.33484, 1.4983, 2.0]):
        start = int((0.002 + i * 0.004) * sr)
        if start >= n:
            continue
        length = min(int(0.05 * sr), n - start)
        t = np.arange(length, dtype=np.float32) / float(sr)
        env = np.exp(-t * (58.0 + i * 10.0))
        f = f0 * mult
        phase = 2.0 * np.pi * f * t
        tone = np.sin(phase) + 0.2 * np.sin(2.0 * phase + 0.4)
        y[start:start + length] += tone * env * 0.15

    t_all = np.arange(n, dtype=np.float32) / float(sr)
    y += 0.025 * np.random.uniform(-1.0, 1.0, n).astype(np.float32) * np.exp(-t_all * 140.0)
    y = np.tanh(y * 1.8)

    return np.stack([y, np.roll(y, 2)], axis=1).astype(np.float32)


if __name__ == "__main__":
    from _render_util import render_cli
    render_cli(generate)
