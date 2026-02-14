import numpy as np


def generate(sr: int, duration: float, context=None):
    n = int(sr * duration)
    if n <= 0:
        return np.zeros((0,), dtype=np.float32)

    y = np.zeros(n, dtype=np.float32)
    # C minor-ish cluster
    freqs = [261.63, 311.13, 392.00, 523.25]

    for k, f in enumerate(freqs):
        start = int((0.002 + k * 0.005) * sr)
        if start >= n:
            continue
        length = min(int((0.07 - k * 0.01) * sr), n - start)
        t = np.arange(length, dtype=np.float32) / float(sr)
        env = np.exp(-t * (45.0 + 9.0 * k))
        tone = np.sin(2.0 * np.pi * f * t)
        tone += 0.32 * np.sin(2.0 * np.pi * (f * 2.7) * t + 0.5)
        y[start:start + length] += tone * env * 0.2

    # Tiny inharmonic sparkle.
    t_all = np.arange(n, dtype=np.float32) / float(sr)
    y += 0.04 * np.sin(2.0 * np.pi * 1700.0 * t_all) * np.exp(-t_all * 100.0)

    y = np.tanh(y * 1.6)
    return np.stack([y, y], axis=1).astype(np.float32)


if __name__ == "__main__":
    from _render_util import render_cli
    render_cli(generate)
