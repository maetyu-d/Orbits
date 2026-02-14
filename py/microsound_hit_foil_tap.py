import numpy as np


def generate(sr: int, duration: float, context=None):
    n = int(sr * duration)
    if n <= 0:
        return np.zeros((0,), dtype=np.float32)

    t = np.arange(n, dtype=np.float32) / float(sr)

    partials = (
        0.45 * np.sin(2.0 * np.pi * 1700.0 * t)
        + 0.29 * np.sin(2.0 * np.pi * 2620.0 * t + 0.4)
        + 0.21 * np.sin(2.0 * np.pi * 4110.0 * t + 0.8)
        + 0.13 * np.sin(2.0 * np.pi * 5380.0 * t + 1.1)
    )

    env = np.exp(-t * 98.0)
    y = partials * env

    edge = np.random.uniform(-1.0, 1.0, n).astype(np.float32)
    y += (edge - np.roll(edge, 1)) * np.exp(-t * 220.0) * 0.07

    y = np.tanh(y * 1.55)
    return np.stack([y, y], axis=1).astype(np.float32)


if __name__ == "__main__":
    from _render_util import render_cli
    render_cli(generate)
