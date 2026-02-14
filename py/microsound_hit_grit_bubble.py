import numpy as np


def generate(sr: int, duration: float, context=None):
    n = int(sr * duration)
    if n <= 0:
        return np.zeros((0,), dtype=np.float32)

    rng = np.random.default_rng()
    y = np.zeros(n, dtype=np.float32)

    # Bubble-like ping cluster.
    for k in range(9):
        start = int((k * 0.0018 + rng.uniform(0.0, 0.0006)) * sr)
        if start >= n:
            break
        length = min(int(0.009 * sr), n - start)
        t = np.arange(length, dtype=np.float32) / float(sr)
        freq = rng.uniform(380.0, 2100.0)
        env = np.exp(-t * rng.uniform(80.0, 170.0))
        y[start:start+length] += np.sin(2.0 * np.pi * freq * t) * env * 0.16

    t = np.arange(n, dtype=np.float32) / float(sr)
    grit = rng.uniform(-1.0, 1.0, n).astype(np.float32) * np.exp(-t * 110.0) * 0.12
    y = np.tanh((y + grit) * 1.8)

    return np.stack([y, y], axis=1).astype(np.float32)


if __name__ == "__main__":
    from _render_util import render_cli
    render_cli(generate)
