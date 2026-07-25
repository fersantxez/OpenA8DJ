#!/usr/bin/env python3
from __future__ import annotations

import argparse
import importlib.util
from pathlib import Path

import numpy as np


PATH = Path(__file__).with_name("analyze_a8dj_timecode_capture.py")
SPEC = importlib.util.spec_from_file_location("timecode_analyzer", PATH)
assert SPEC and SPEC.loader
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


def args() -> argparse.Namespace:
    return argparse.Namespace(
        edge_trim_seconds=0.0,
        min_rms=0.02,
        max_balance_db=6.0,
        max_abs_correlation=0.98,
        min_phase_ratio=0.05,
        click_threshold=0.075,
        clip_threshold=0.999,
    )


rate = 48000
t = np.arange(rate * 2, dtype=np.float64) / rate
good = np.column_stack((0.2 * np.sin(2 * np.pi * 2000 * t), 0.2 * np.cos(2 * np.pi * 2000 * t)))
assert MODULE.pair_metrics(good, rate, 0, args())["passed"]

quiet = good * 0.01
assert not MODULE.pair_metrics(quiet, rate, 0, args())["passed"]

mono = np.column_stack((good[:, 0], good[:, 0]))
assert not MODULE.pair_metrics(mono, rate, 0, args())["passed"]

clicked = good.copy()
clicked[1000, 0] = 0.9
assert not MODULE.pair_metrics(clicked, rate, 0, args())["passed"]

print("PASS: timecode capture analyzer contracts")
