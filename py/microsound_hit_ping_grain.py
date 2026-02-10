import numpy as np


def generate(sr: int, duration: float, context=None):
    n = int(sr * duration)
    if n <= 0:
        return np.zeros((0,), dtype=np.float32)

    t = np.arange(n, dtype=np.float32) / float(sr)

    f = 760.0
    ping = np.sin(2.0 * np.pi * f * t) * np.exp(-t * 65.0)

    grain_len = max(1, int(0.012 * sr))
    grain = np.sin(2.0 * np.pi * 3120.0 * np.arange(grain_len, dtype=np.float32) / float(sr))
    grain *= np.hanning(grain_len).astype(np.float32)

    y = ping.astype(np.float32)
    for start in (int(0.002 * sr), int(0.009 * sr), int(0.017 * sr)):
        end = min(n, start + grain_len)
        y[start:end] += grain[: end - start] * 0.32

    y = np.tanh(y * 1.1)
    return np.stack([y, y], axis=1).astype(np.float32)


if __name__ == "__main__":
    from _render_util import render_cli
    render_cli(generate)
