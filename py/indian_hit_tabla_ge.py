import numpy as np


def _env(n: int, a: float = 0.001, d: float = 0.28, sr: int = 48000):
    t = np.arange(n, dtype=np.float32) / float(sr)
    attack = np.clip(t / max(a, 1e-5), 0.0, 1.0)
    decay = np.exp(-t / max(d, 1e-5))
    return attack * decay


def generate(sr: int, duration: float, context=None):
    n = max(0, int(sr * duration))
    if n <= 0:
        return np.zeros((0,), dtype=np.float32)

    hit_len = min(n, int(0.38 * sr))
    t = np.arange(hit_len, dtype=np.float32) / float(sr)

    f0 = 165.0
    pitch_drop = 1.0 - 0.22 * np.clip(t / 0.09, 0.0, 1.0)
    ph = 2.0 * np.pi * np.cumsum((f0 * pitch_drop) / float(sr)).astype(np.float32)

    bass = 0.82 * np.sin(ph)
    low2 = 0.22 * np.sin(2.0 * ph + 0.4)
    knock = 0.12 * np.sin(2.0 * np.pi * 430.0 * t)

    y_hit = (bass + low2 + knock) * _env(hit_len, a=0.0012, d=0.24, sr=sr)

    y = np.zeros((n,), dtype=np.float32)
    y[:hit_len] = y_hit
    y *= 0.9

    stereo = np.stack([y, y], axis=1).astype(np.float32)
    return stereo


if __name__ == "__main__":
    from _render_util import render_cli

    render_cli(generate)
