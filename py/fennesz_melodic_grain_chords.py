import numpy as np


def generate(sr: int, duration: float, context=None):
    n = int(sr * duration)
    if n <= 0:
        return np.zeros((0,), dtype=np.float32)

    rng = np.random.default_rng(31)
    y = np.zeros(n, dtype=np.float32)

    base = [196.0, 246.94, 293.66, 392.0]
    grain_count = max(12, int(duration * 26))

    for _ in range(grain_count):
        start = int(rng.uniform(0.0, max(0.001, duration * 0.85)) * sr)
        length = max(16, int(rng.uniform(0.01, 0.06) * sr))
        end = min(n, start + length)
        L = end - start
        if L <= 0:
            continue

        f = base[int(rng.integers(0, len(base)))] * (2.0 ** (rng.choice([-12, 0, 7]) / 12.0))
        tt = np.arange(L, dtype=np.float32) / float(sr)
        env = np.hanning(max(8, L)).astype(np.float32)
        tone = np.sin(2.0 * np.pi * f * tt + rng.uniform(0.0, 1.0))
        tone += 0.18 * np.sin(2.0 * np.pi * 1.99 * f * tt)
        y[start:end] += tone[:L] * env[:L] * 0.2

    y += rng.uniform(-1.0, 1.0, n).astype(np.float32) * 0.018
    y = np.tanh(y * 1.35).astype(np.float32)

    return np.stack([y, y], axis=1)


if __name__ == "__main__":
    from _render_util import render_cli
    render_cli(generate)
