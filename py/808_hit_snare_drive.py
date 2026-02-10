import numpy as np


def generate(sr: int, duration: float, context=None):
    n = int(sr * duration)
    if n <= 0:
        return np.zeros((0,), dtype=np.float32)

    t = np.arange(n, dtype=np.float32) / float(sr)
    noise = np.random.uniform(-1.0, 1.0, n).astype(np.float32)
    # Snappier transient, shorter tonal ring.
    env_noise = np.exp(-t * 52.0)
    env_tone = np.exp(-t * 62.0)
    tone = (np.sin(2.0 * np.pi * 190.0 * t) + 0.22 * np.sin(2.0 * np.pi * 360.0 * t)) * env_tone

    # Front-loaded crack: high-frequency burst with very fast decay.
    snap = (noise - np.concatenate(([0.0], noise[:-1])) * 0.92) * np.exp(-t * 320.0)
    snap2 = np.sin(2.0 * np.pi * 4200.0 * t) * np.exp(-t * 260.0)

    y = noise * env_noise * 0.46 + tone * 0.24 + snap * 0.78 + snap2 * 0.28
    y = np.tanh(y * 1.75) * 0.68
    return np.stack([y, y], axis=1).astype(np.float32)


if __name__ == "__main__":
    from _render_util import render_cli
    render_cli(generate)
