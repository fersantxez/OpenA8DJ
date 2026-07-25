#!/usr/bin/env python3
"""Test the deterministic input-to-output latency fixture."""

from __future__ import annotations

import importlib.machinery
import importlib.util
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "scripts/timecode-latency-e2e-fixture"
LOADER = importlib.machinery.SourceFileLoader("timecode_latency_fixture", str(SOURCE))
SPEC = importlib.util.spec_from_loader("timecode_latency_fixture", LOADER)
if SPEC is None or SPEC.loader is None:
    raise SystemExit("cannot load latency fixture")
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


def main() -> int:
    baseline = MODULE.simulate(
        name="baseline",
        sample_rate=48000,
        capture_iso_frames=64,
        host_buffer_frames=512,
        output_target_frames=3072,
        output_prefetch_frames=128,
        start_latency_frames=3072,
        restart_latency_frames=1536,
    )
    prefetch = MODULE.simulate(
        name="prefetch64",
        sample_rate=48000,
        capture_iso_frames=64,
        host_buffer_frames=512,
        output_target_frames=3072,
        output_prefetch_frames=64,
        start_latency_frames=3072,
        restart_latency_frames=1536,
    )
    restart = MODULE.simulate(
        name="restart1024",
        sample_rate=48000,
        capture_iso_frames=64,
        host_buffer_frames=512,
        output_target_frames=3072,
        output_prefetch_frames=128,
        start_latency_frames=3072,
        restart_latency_frames=1024,
    )

    assert baseline["result"] == "PASS"
    assert prefetch["result"] == "PASS"
    assert restart["result"] == "PASS"
    assert baseline["physical_evidence_present"] is False
    assert baseline["product_claim_allowed"] is False
    assert prefetch["latency_ms"]["p95"] < baseline["latency_ms"]["p95"]
    assert restart["response_phases_ms"]["restart_p50"] < baseline["response_phases_ms"]["restart_p50"]
    assert restart["response_phases_ms"]["steady_p95"] == baseline["response_phases_ms"]["steady_p95"]
    assert all(item["result"] == "PASS" for item in baseline["gates"])

    print("timecode_latency_e2e_fixture_test=PASS")
    print(f"baseline_p95_ms={baseline['latency_ms']['p95']:.6f}")
    print(f"prefetch64_p95_ms={prefetch['latency_ms']['p95']:.6f}")
    print(f"restart1024_restart_p50_ms={restart['response_phases_ms']['restart_p50']:.6f}")
    print("physical_evidence_present=False")
    print("product_claim_allowed=False")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
