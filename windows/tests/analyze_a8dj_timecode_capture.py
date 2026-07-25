#!/usr/bin/env python3
"""Conservative prequalification gate for stereo DVS/timecode captures.

This gate only decides whether a captured stereo pair is plausible input for a
timecode decoder.  It deliberately does not claim Traktor calibration success;
that still requires the application's signal meter and stable scope.
"""

from __future__ import annotations

import argparse
import json
import math
from pathlib import Path

import numpy as np
import soundfile as sf


def pair_metrics(audio: np.ndarray, rate: int, pair: int, args: argparse.Namespace) -> dict:
    start = pair * 2
    stereo = audio[:, start : start + 2]
    if stereo.shape[1] != 2:
        return {"pair": pair + 1, "passed": False, "reasons": ["missing_stereo_channels"]}

    trim = min(len(stereo) // 4, int(rate * args.edge_trim_seconds))
    active = stereo[trim : len(stereo) - trim] if trim and len(stereo) > 2 * trim else stereo
    peak = np.max(np.abs(active), axis=0) if active.size else np.zeros(2)
    rms = np.sqrt(np.mean(np.square(active), axis=0)) if active.size else np.zeros(2)
    rms_floor = np.maximum(rms, 1.0e-12)
    balance_db = float(abs(20.0 * math.log10(float(rms_floor[0] / rms_floor[1]))))
    corr = float(np.corrcoef(active[:, 0], active[:, 1])[0, 1]) if len(active) > 2 else 1.0
    if not math.isfinite(corr):
        corr = 1.0

    centered = active - np.mean(active, axis=0, keepdims=True) if active.size else active
    covariance = np.cov(centered, rowvar=False) if len(centered) > 2 else np.zeros((2, 2))
    eigenvalues = np.sort(np.maximum(np.linalg.eigvalsh(covariance), 0.0))
    phase_minor_major_ratio = float(eigenvalues[0] / max(eigenvalues[1], 1.0e-24))

    diff = np.diff(active, axis=0) if len(active) > 1 else np.zeros((0, 2))
    raw_clicks = int(np.count_nonzero(np.max(np.abs(diff), axis=1) > args.click_threshold))
    clipped_frames = int(np.count_nonzero(np.max(np.abs(active), axis=1) >= args.clip_threshold))

    reasons: list[str] = []
    for channel, value in enumerate(rms):
        if float(value) < args.min_rms:
            reasons.append(f"channel_{channel + 1}_rms_below_{args.min_rms}")
    if balance_db > args.max_balance_db:
        reasons.append(f"stereo_imbalance_db_{balance_db:.3f}")
    if abs(corr) > args.max_abs_correlation:
        reasons.append(f"stereo_abs_correlation_{abs(corr):.6f}")
    if phase_minor_major_ratio < args.min_phase_ratio:
        reasons.append(f"phase_ratio_{phase_minor_major_ratio:.6f}")
    if raw_clicks:
        reasons.append(f"raw_clicks_{raw_clicks}")
    if clipped_frames:
        reasons.append(f"clipped_frames_{clipped_frames}")

    return {
        "pair": pair + 1,
        "peak": peak.astype(float).tolist(),
        "rms": rms.astype(float).tolist(),
        "rms_dbfs": [float(20.0 * math.log10(max(float(value), 1.0e-12))) for value in rms],
        "balance_db": balance_db,
        "stereo_correlation": corr,
        "phase_minor_major_ratio": phase_minor_major_ratio,
        "raw_clicks": raw_clicks,
        "clipped_frames": clipped_frames,
        "passed": not reasons,
        "reasons": reasons,
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--wav", required=True, type=Path)
    parser.add_argument("--out", type=Path)
    parser.add_argument("--pairs", default="1,2", help="One-based stereo pair numbers.")
    parser.add_argument("--min-rms", type=float, default=0.02)
    parser.add_argument("--max-balance-db", type=float, default=6.0)
    parser.add_argument("--max-abs-correlation", type=float, default=0.98)
    parser.add_argument("--min-phase-ratio", type=float, default=0.05)
    parser.add_argument("--click-threshold", type=float, default=0.075)
    parser.add_argument("--clip-threshold", type=float, default=0.999)
    parser.add_argument("--edge-trim-seconds", type=float, default=0.25)
    args = parser.parse_args()

    audio, rate = sf.read(args.wav, dtype="float64", always_2d=True)
    pairs = [int(value.strip()) - 1 for value in args.pairs.split(",") if value.strip()]
    results = [pair_metrics(audio, rate, pair, args) for pair in pairs]
    passed = bool(results) and all(bool(result["passed"]) for result in results)
    summary = {
        "wav": str(args.wav),
        "rate": rate,
        "frames": len(audio),
        "channels": audio.shape[1],
        "pairs": results,
        "timecode_signal_prequalification": "PASS" if passed else "FAIL",
        "traktor_calibration_proven": False,
        "scope_requirement": "Traktor signal meter full and stable two-circle scope for decks A and B.",
    }
    text = json.dumps(summary, indent=2) + "\n"
    if args.out:
        args.out.parent.mkdir(parents=True, exist_ok=True)
        args.out.write_text(text, encoding="utf-8")
    print(text, end="")
    return 0 if passed else 2


if __name__ == "__main__":
    raise SystemExit(main())
