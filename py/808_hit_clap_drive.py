import numpy as np


def generate(sr: int, duration: float, context=None):
    n = int(sr * duration)
    if n <= 0:
        return np.zeros((0,), dtype=np.float32)

    y = np.zeros(n, dtype=np.float32)
    t = np.arange(n, dtype=np.float32) / float(sr)
    noise = np.random.uniform(-1.0, 1.0, n).astype(np.float32)

    for d, amp in [(0.0, 1.0), (0.010, 0.82), (0.020, 0.7)]:
        start = int(d * sr)
        if start >= n:
            continue
        tt = t[: n - start]
        env = np.exp(-tt * 75.0)
        y[start:] += noise[: n - start] * env * amp

    tail = noise * np.exp(-t * 11.0) * 0.28
    y = y + tail
    y = np.tanh(y * 1.35) * 0.7
    return np.stack([y, y], axis=1).astype(np.float32)


if __name__ == "__main__":
    from _render_util import render_cli
    render_cli(generate)
