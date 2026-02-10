import numpy as np


def _env(n: int, a: float = 0.001, d: float = 0.14, sr: int = 48000):
    t = np.arange(n, dtype=np.float32) / float(sr)
    attack = np.clip(t / max(a, 1e-5), 0.0, 1.0)
    decay = np.exp(-t / max(d, 1e-5))
    return attack * decay


def generate(sr: int, duration: float, context=None):
    n = max(0, int(sr * duration))
    if n <= 0:
        return np.zeros((0,), dtype=np.float32)

    hit_len = min(n, int(0.22 * sr))
    t = np.arange(hit_len, dtype=np.float32) / float(sr)

    body = 0.55 * np.sin(2.0 * np.pi * 760.0 * t)
    edge = 0.20 * np.sin(2.0 * np.pi * 1510.0 * t + 0.25)
    bright = 0.10 * np.sin(2.0 * np.pi * 2350.0 * t + 0.9)

    y_hit = (body + edge + bright) * _env(hit_len, a=0.0008, d=0.10, sr=sr)

    y = np.zeros((n,), dtype=np.float32)
    y[:hit_len] = y_hit
    y *= 0.85

    stereo = np.stack([y, y], axis=1).astype(np.float32)
    return stereo


if __name__ == "__main__":
    from _render_util import render_cli

    render_cli(generate)
