import numpy as np


def generate(sr: int, duration: float, context=None):
    n = int(sr * duration)
    if n <= 0:
        return np.zeros((0,), dtype=np.float32)

    rng = np.random.default_rng()
    t = np.arange(n, dtype=np.float32) / float(sr)

    noise = rng.uniform(-1.0, 1.0, n).astype(np.float32)
    hp = noise - np.concatenate(([0.0], noise[:-1]))

    # Pitch-swept thread.
    f0, f1 = 5200.0, 1300.0
    sweep = np.linspace(f0, f1, n, dtype=np.float32)
    ph = 2.0 * np.pi * np.cumsum(sweep / float(sr)).astype(np.float32)
    thread = np.sin(ph)

    y = hp * np.exp(-t * 145.0) * 0.22 + thread * np.exp(-t * 95.0) * 0.20
    y = np.tanh(y * 1.9)

    l = y
    r = np.roll(y, 3)
    return np.stack([l, r], axis=1).astype(np.float32)


if __name__ == "__main__":
    from _render_util import render_cli
    render_cli(generate)
