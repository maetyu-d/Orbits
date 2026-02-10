import numpy as np


def generate(sr: int, duration: float, context=None):
    n = max(0, int(sr * duration))
    if n <= 0:
        return np.zeros((0,), dtype=np.float32)

    hit_len = min(n, int(0.75 * sr))
    t = np.arange(hit_len, dtype=np.float32) / float(sr)

    partials = (
        0.44 * np.sin(2.0 * np.pi * 1500.0 * t)
        + 0.30 * np.sin(2.0 * np.pi * 2310.0 * t + 0.2)
        + 0.22 * np.sin(2.0 * np.pi * 3160.0 * t + 0.55)
        + 0.14 * np.sin(2.0 * np.pi * 4760.0 * t + 1.0)
    )
    env = np.exp(-t / 0.22)
    y_hit = partials * env

    y = np.zeros((n,), dtype=np.float32)
    y[:hit_len] = y_hit
    y *= 0.62

    stereo = np.stack([y, y], axis=1).astype(np.float32)
    return stereo


if __name__ == "__main__":
    from _render_util import render_cli

    render_cli(generate)
