import numpy as np


def generate(sr: int, duration: float, context=None):
    n = int(sr * duration)
    if n <= 0:
        return np.zeros((0,), dtype=np.float32)

    t = np.arange(n, dtype=np.float32) / float(sr)
    noise = np.random.uniform(-1.0, 1.0, n).astype(np.float32)
    hp = noise - np.concatenate(([0.0], noise[:-1])) * 0.94
    env = np.exp(-t * 45.0)

    y = hp * env * 0.95
    y = np.tanh(y * 1.55) * 0.62
    return np.stack([y, y], axis=1).astype(np.float32)


if __name__ == "__main__":
    from _render_util import render_cli
    render_cli(generate)
