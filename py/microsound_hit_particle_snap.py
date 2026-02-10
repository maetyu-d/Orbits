import numpy as np


def generate(sr: int, duration: float, context=None):
    n = int(sr * duration)
    if n <= 0:
        return np.zeros((0,), dtype=np.float32)

    y = np.zeros(n, dtype=np.float32)

    # A few microscopic particles clustered near onset.
    rng = np.random.default_rng()
    for _ in range(7):
        start = int(rng.uniform(0.0, 0.02) * sr)
        length = max(4, int(rng.uniform(0.001, 0.006) * sr))
        freq = rng.uniform(900.0, 5200.0)

        end = min(n, start + length)
        L = end - start
        if L <= 0:
            continue

        tt = np.arange(L, dtype=np.float32) / float(sr)
        env = np.hanning(max(4, L)).astype(np.float32)
        tone = np.sin(2.0 * np.pi * freq * tt)
        y[start:end] += tone[:L] * env[:L] * 0.32

    # Tiny noisy impulse body.
    t = np.arange(n, dtype=np.float32) / float(sr)
    y += np.random.uniform(-1.0, 1.0, n).astype(np.float32) * np.exp(-t * 150.0) * 0.15

    y = np.tanh(y * 1.8)
    return np.stack([y, y], axis=1).astype(np.float32)


if __name__ == "__main__":
    from _render_util import render_cli
    render_cli(generate)
