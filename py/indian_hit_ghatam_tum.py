import numpy as np


def generate(sr: int, duration: float, context=None):
    n = max(0, int(sr * duration))
    if n <= 0:
        return np.zeros((0,), dtype=np.float32)

    hit_len = min(n, int(0.42 * sr))
    t = np.arange(hit_len, dtype=np.float32) / float(sr)

    f1 = 180.0
    f2 = 265.0
    f3 = 410.0

    y_hit = (
        0.72 * np.sin(2.0 * np.pi * f1 * t)
        + 0.32 * np.sin(2.0 * np.pi * f2 * t + 0.45)
        + 0.20 * np.sin(2.0 * np.pi * f3 * t + 0.9)
    )

    y_hit *= np.exp(-t / 0.17)
    y_hit += 0.07 * np.sin(2.0 * np.pi * 1700.0 * t) * np.exp(-t / 0.02)

    y = np.zeros((n,), dtype=np.float32)
    y[:hit_len] = y_hit
    y *= 0.82

    stereo = np.stack([y, y], axis=1).astype(np.float32)
    return stereo


if __name__ == "__main__":
    from _render_util import render_cli

    render_cli(generate)
