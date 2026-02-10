import numpy as np


def generate(sr: int, duration: float, context=None):
    n = max(0, int(sr * duration))
    if n <= 0:
        return np.zeros((0,), dtype=np.float32)

    hit_len = min(n, int(0.18 * sr))
    t = np.arange(hit_len, dtype=np.float32) / float(sr)

    noise = np.random.RandomState(7).randn(hit_len).astype(np.float32)
    hp = noise - np.concatenate(([0.0], noise[:-1]))

    tone = 0.28 * np.sin(2.0 * np.pi * 520.0 * t)
    shell = 0.20 * np.sin(2.0 * np.pi * 980.0 * t + 0.3)
    env = np.exp(-t / 0.06)

    y_hit = (0.20 * hp + tone + shell) * env

    y = np.zeros((n,), dtype=np.float32)
    y[:hit_len] = y_hit
    y *= 0.8

    l = y
    r = np.roll(y, 2)
    stereo = np.stack([l, r], axis=1).astype(np.float32)
    return stereo


if __name__ == "__main__":
    from _render_util import render_cli

    render_cli(generate)
