import numpy as np


def generate(sr: int, duration: float, context=None):
    n = int(sr * duration)
    if n <= 0:
        return np.zeros((0,), dtype=np.float32)

    y = np.zeros(n, dtype=np.float32)
    rng = np.random.default_rng(44)

    seq = [220.0, 277.18, 329.63, 440.0, 329.63, 277.18]
    step = max(1, int(0.055 * sr))

    for i in range(int(np.ceil(n / step))):
        f = seq[i % len(seq)] * (1.0 + rng.uniform(-0.002, 0.002))
        start = i * step
        end = min(n, start + int(0.11 * sr))
        L = end - start
        if L <= 0:
            break
        t = np.arange(L, dtype=np.float32) / float(sr)
        env = np.exp(-t * 24.0)
        tone = np.sin(2.0 * np.pi * f * t)
        tone += 0.25 * np.sin(2.0 * np.pi * (f * 2.0) * t + 0.3)
        y[start:end] += tone * env * 0.17

    t_all = np.arange(n, dtype=np.float32) / float(sr)
    y *= 0.55 + 0.45 * np.sin(2.0 * np.pi * 0.06 * t_all + 0.4)
    y += rng.uniform(-1.0, 1.0, n).astype(np.float32) * 0.02
    y = np.tanh(y * 1.32).astype(np.float32)

    return np.stack([np.roll(y, 4), y], axis=1)


if __name__ == "__main__":
    from _render_util import render_cli
    render_cli(generate)
