import numpy as np


def generate(sr: int, duration: float, context=None):
    n = int(sr * duration)
    if n <= 0:
        return np.zeros((0,), dtype=np.float32)

    t = np.arange(n, dtype=np.float32) / float(sr)
    rng = np.random.default_rng()

    # Chirped micro-tone.
    f0, f1 = 900.0, 6600.0
    sweep = np.linspace(f0, f1, n, dtype=np.float32)
    phase = 2.0 * np.pi * np.cumsum(sweep / float(sr)).astype(np.float32)
    chirp = np.sin(phase) * np.exp(-t * 120.0) * 0.25

    # Static burst.
    static = rng.uniform(-1.0, 1.0, n).astype(np.float32)
    static *= np.exp(-t * 175.0) * 0.24

    y = np.tanh((chirp + static) * 1.6)
    l = y
    r = np.roll(y, 1)
    return np.stack([l, r], axis=1).astype(np.float32)


if __name__ == "__main__":
    from _render_util import render_cli
    render_cli(generate)
