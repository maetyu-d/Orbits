import numpy as np


def generate(sr: int, duration: float, context=None):
    n = int(sr * duration)
    if n <= 0:
        return np.zeros((0,), dtype=np.float32)

    t = np.arange(n, dtype=np.float32) / float(sr)

    imp = np.zeros(n, dtype=np.float32)
    imp[0:min(4, n)] = 1.0

    # Resonant micro-body.
    body = np.zeros(n, dtype=np.float32)
    freqs = [920.0, 1730.0, 2410.0, 3590.0]
    gains = [0.55, 0.34, 0.25, 0.16]
    for f, g in zip(freqs, gains):
        body += g * np.sin(2.0 * np.pi * f * t + (f * 0.0017))

    env = np.exp(-t * 72.0)
    y = body * env

    # Fine transient edge.
    edge = (np.random.uniform(-1.0, 1.0, n).astype(np.float32) - np.roll(np.random.uniform(-1.0, 1.0, n).astype(np.float32), 1))
    y += edge * np.exp(-t * 210.0) * 0.1

    y = np.tanh(y * 1.7)
    return np.stack([y, y], axis=1).astype(np.float32)


if __name__ == "__main__":
    from _render_util import render_cli
    render_cli(generate)
