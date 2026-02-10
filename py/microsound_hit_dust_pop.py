import numpy as np


def generate(sr: int, duration: float, context=None):
    n = int(sr * duration)
    if n <= 0:
        return np.zeros((0,), dtype=np.float32)

    t = np.arange(n, dtype=np.float32) / float(sr)
    noise = np.random.uniform(-1.0, 1.0, n).astype(np.float32)

    env_fast = np.exp(-t * 180.0)
    env_tail = np.exp(-t * 42.0)

    burst = noise * env_fast
    click = np.sin(2.0 * np.pi * 4800.0 * t) * env_fast * 0.35
    hiss_tail = noise * env_tail * 0.18

    y = (burst * 0.8) + click + hiss_tail
    y = np.tanh(y * 1.3)
    return np.stack([y, y], axis=1).astype(np.float32)


if __name__ == "__main__":
    from _render_util import render_cli
    render_cli(generate)
