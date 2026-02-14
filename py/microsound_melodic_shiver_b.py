import numpy as np


def generate(sr: int, duration: float, context=None):
    n = int(sr * duration)
    if n <= 0:
        return np.zeros((0,), dtype=np.float32)

    rng = np.random.default_rng()
    y = np.zeros(n, dtype=np.float32)

    root = 246.94  # B3
    semis = [0, 4, 7, 11, 14]

    for i in range(12):
        st = semis[i % len(semis)]
        f = root * (2.0 ** (st / 12.0))
        start = int(rng.uniform(0.0, 0.05) * sr)
        length = max(8, int(rng.uniform(0.008, 0.028) * sr))
        end = min(n, start + length)
        L = end - start
        if L <= 0:
            continue
        t = np.arange(L, dtype=np.float32) / float(sr)
        env = np.exp(-t * rng.uniform(70.0, 150.0))
        vib = 1.0 + 0.006 * np.sin(2.0 * np.pi * 23.0 * t)
        ph = 2.0 * np.pi * np.cumsum((f * vib) / float(sr)).astype(np.float32)
        grain = np.sin(ph) + 0.16 * np.sin(2.0 * ph + 0.4)
        y[start:end] += grain * env * 0.10

    t_all = np.arange(n, dtype=np.float32) / float(sr)
    y += (rng.uniform(-1.0, 1.0, n).astype(np.float32) - np.roll(rng.uniform(-1.0, 1.0, n).astype(np.float32), 1)) * np.exp(-t_all * 190.0) * 0.015
    y = np.tanh(y * 1.9)

    return np.stack([np.roll(y, 1), y], axis=1).astype(np.float32)


if __name__ == "__main__":
    from _render_util import render_cli
    render_cli(generate)
