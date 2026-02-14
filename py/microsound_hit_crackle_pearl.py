import numpy as np


def generate(sr: int, duration: float, context=None):
    n = int(sr * duration)
    if n <= 0:
        return np.zeros((0,), dtype=np.float32)

    rng = np.random.default_rng()
    y = np.zeros(n, dtype=np.float32)

    for _ in range(14):
        start = int(rng.uniform(0.0, 0.03) * sr)
        length = max(6, int(rng.uniform(0.0008, 0.004) * sr))
        end = min(n, start + length)
        L = end - start
        if L <= 0:
            continue
        t = np.arange(L, dtype=np.float32) / float(sr)
        f = rng.uniform(1400.0, 7800.0)
        env = np.exp(-t * rng.uniform(260.0, 900.0))
        y[start:end] += np.sin(2.0 * np.pi * f * t) * env * rng.uniform(0.05, 0.22)

    t = np.arange(n, dtype=np.float32) / float(sr)
    dust = rng.uniform(-1.0, 1.0, n).astype(np.float32) * np.exp(-t * 120.0) * 0.08
    y = np.tanh((y + dust) * 2.4)
    return np.stack([y, y], axis=1).astype(np.float32)


if __name__ == "__main__":
    from _render_util import render_cli
    render_cli(generate)
