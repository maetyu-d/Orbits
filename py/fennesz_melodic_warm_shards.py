import numpy as np


def generate(sr: int, duration: float, context=None):
    n = int(sr * duration)
    if n <= 0:
        return np.zeros((0,), dtype=np.float32)

    rng = np.random.default_rng(72)
    y = np.zeros(n, dtype=np.float32)

    scale = [233.08, 261.63, 311.13, 349.23, 466.16]
    count = max(10, int(duration * 18))

    for _ in range(count):
        start = int(rng.uniform(0.0, max(0.001, duration * 0.9)) * sr)
        length = max(24, int(rng.uniform(0.02, 0.08) * sr))
        end = min(n, start + length)
        L = end - start
        if L <= 0:
            continue

        f = scale[int(rng.integers(0, len(scale)))]
        tt = np.arange(L, dtype=np.float32) / float(sr)
        env = np.exp(-tt * rng.uniform(18.0, 42.0))
        tone = np.sin(2.0 * np.pi * f * tt + rng.uniform(0.0, 1.0))
        tone += 0.2 * np.sin(2.0 * np.pi * (f * 1.5) * tt)
        y[start:end] += tone * env * 0.16

    t = np.arange(n, dtype=np.float32) / float(sr)
    y *= 0.58 + 0.42 * np.sin(2.0 * np.pi * 0.07 * t - 0.2)
    y += rng.uniform(-1.0, 1.0, n).astype(np.float32) * 0.015
    y = np.tanh(y * 1.3).astype(np.float32)

    return np.stack([np.roll(y, 5), y], axis=1)


if __name__ == "__main__":
    from _render_util import render_cli
    render_cli(generate)
