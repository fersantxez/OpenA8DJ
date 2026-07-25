#!/usr/bin/env python3
"""Test the deterministic latency frontier model without touching audio hardware."""

from __future__ import annotations

import importlib.machinery
import importlib.util
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "scripts/timecode-latency-offline-gate"
LOADER = importlib.machinery.SourceFileLoader("timecode_latency_gate", str(SOURCE))
SPEC = importlib.util.spec_from_loader("timecode_latency_gate", LOADER)
if SPEC is None or SPEC.loader is None:
    raise SystemExit("cannot load latency gate")
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


def main() -> int:
    baseline = MODULE.model_profile("baseline", 48000, 64, 3072, 512, 128)
    prefetch_64 = MODULE.model_profile("prefetch64", 48000, 64, 3072, 512, 64)
    output_2048 = MODULE.model_profile("output2048", 48000, 64, 2048, 512, 128)
    output_2816 = MODULE.model_profile("output2816", 48000, 64, 2816, 512, 128)
    host_256 = MODULE.model_profile("host256", 48000, 64, 2048, 256, 128)
    unsafe = MODULE.model_profile("unsafe", 48000, 64, 1024, 512, 128)

    assert baseline["result"] == "PASS"
    assert prefetch_64["result"] == "PASS"
    assert output_2048["result"] == "PASS"
    assert output_2816["result"] == "PASS"
    assert host_256["result"] == "PASS"
    assert unsafe["result"] == "FAIL"
    assert output_2048["modeled_pipeline_p95_ms"] < baseline["modeled_pipeline_p95_ms"]
    assert prefetch_64["modeled_pipeline_p95_ms"] < baseline["modeled_pipeline_p95_ms"]
    assert prefetch_64["realtime_work_proxy_per_second"] == baseline["realtime_work_proxy_per_second"]
    assert output_2816["modeled_pipeline_p95_ms"] < baseline["modeled_pipeline_p95_ms"]
    assert output_2816["modeled_pipeline_p95_ms"] > output_2048["modeled_pipeline_p95_ms"]
    assert host_256["modeled_pipeline_p95_ms"] < output_2048["modeled_pipeline_p95_ms"]
    assert output_2048["geometry_safe_for_frontier"] is True
    assert unsafe["geometry_safe_for_frontier"] is False
    assert MODULE.compare_profiles([baseline, output_2048])["model_only"] is True

    print("timecode_latency_offline_gate_test=PASS")
    print("physical_evidence_present=False")
    print("product_claim_allowed=False")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
