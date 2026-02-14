import numpy as np


def generate(sr: int, duration: float, context=None):
    n = int(sr * duration)
    if n <= 0:
        return np.zeros((0,), dtype=np.float32)

    rng = np.random.default_rng(23)
    t = np.arange(n, dtype=np.float32) / float(sr)
    y = np.zeros(n, dtype=np.float32)

    notes = [174.61, 220.0, 261.63, 329.63]
    for i, f in enumerate(notes):
        flutter = 1.0 + 0.0045 * np.sin(2.0 * np.pi * (0.6 + 0.17 * i) * t + 0.2 * i)
        phase = 2.0 * np.pi * np.cumsum((f * flutter) / float(sr)).astype(np.float32)
        s = np.sin(phase) + 0.22 * np.sin(2.0 * phase + 0.5)
        y += s * (0.11 / (1 + i * 0.2))

    # tape-like grit
    grit = (rng.uniform(-1.0, 1.0, n).astype(np.float32) - np.roll(rng.uniform(-1.0, 1.0, n).astype(np.float32), 1))
    y += grit * 0.028

    swell = 0.5 + 0.5 * np.sin(2.0 * np.pi * 0.09 * t - 0.7)
    y *= 0.65 + 0.35 * swell
    y = np.tanh(y * 1.28).astype(np.float32)

    return np.stack([np.roll(y, 3), y], axis=1)


if __name__ == "__main__":
    from _render_util import render_cli
    render_cli(generate)
