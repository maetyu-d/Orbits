import numpy as np


def _env(n: int, a: float, d: float, sr: int):
    t = np.arange(n, dtype=np.float32) / float(sr)
    return np.clip(t / max(a, 1e-5), 0.0, 1.0) * np.exp(-t / max(d, 1e-5))


def generate(sr: int, duration: float, context=None):
    n = max(0, int(sr * duration))
    if n <= 0:
        return np.zeros((0,), dtype=np.float32)

    hit_len = min(n, int(0.26 * sr))
    t = np.arange(hit_len, dtype=np.float32) / float(sr)

    mid = 0.50 * np.sin(2.0 * np.pi * 320.0 * t)
    ring = 0.25 * np.sin(2.0 * np.pi * 620.0 * t + 0.5)
    click = 0.18 * np.sin(2.0 * np.pi * 2100.0 * t) * np.exp(-t / 0.012)

    y_hit = (mid + ring) * _env(hit_len, a=0.0009, d=0.13, sr=sr) + click

    y = np.zeros((n,), dtype=np.float32)
    y[:hit_len] = y_hit
    y *= 0.84

    stereo = np.stack([y, y], axis=1).astype(np.float32)
    return stereo


if __name__ == "__main__":
    from _render_util import render_cli

    render_cli(generate)
