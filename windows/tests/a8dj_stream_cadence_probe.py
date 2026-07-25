#!/usr/bin/env python3
"""Measure independent WASAPI render/capture callback cadence without I/O files."""

from __future__ import annotations

import argparse
import json
import math
import subprocess
import time
from pathlib import Path

import sounddevice as sd

try:
    import psutil
except ImportError:  # pragma: no cover - the Windows QA environment requires it
    psutil = None

from a8dj_duplex_soak import find_device, write_json


def percentile(values: list[float], percent: float) -> float:
    if not values:
        return 0.0
    ordered = sorted(values)
    position = (len(ordered) - 1) * (percent / 100.0)
    lower = int(math.floor(position))
    upper = int(math.ceil(position))
    if lower == upper:
        return float(ordered[lower])
    return float(
        ordered[lower] * (upper - position)
        + ordered[upper] * (position - lower)
    )


def summarize_cpu(rows: list[dict[str, float]]) -> dict[str, dict[str, float]]:
    keys = (
        "total_cpu",
        "probe_process_cpu",
        "audiodg_cpu",
        "system_process_cpu",
        "interrupt_cpu",
        "dpc_cpu",
    )
    summary: dict[str, dict[str, float]] = {}
    for key in keys:
        values = [float(row[key]) for row in rows if key in row]
        if values:
            summary[key] = {
                "avg": float(sum(values) / len(values)),
                "p95": percentile(values, 95.0),
                "max": float(max(values)),
            }
    summary["sample_count"] = {"value": float(len(rows))}
    return summary


class CpuMonitor:
    def __init__(self) -> None:
        self.available = psutil is not None
        self.probe = None
        self.system = None
        self.audiodg = []
        if not self.available:
            return
        self.probe = psutil.Process()
        try:
            self.system = psutil.Process(4)
        except (psutil.NoSuchProcess, psutil.AccessDenied):
            self.system = None
        self.refresh_audiodg()
        self.probe.cpu_percent(interval=None)
        if self.system is not None:
            self.system.cpu_percent(interval=None)
        for process in self.audiodg:
            process.cpu_percent(interval=None)
        psutil.cpu_percent(interval=None)
        psutil.cpu_times_percent(interval=None)

    def refresh_audiodg(self) -> None:
        if not self.available:
            return
        self.audiodg = []
        for process in psutil.process_iter(["name"]):
            try:
                if str(process.info["name"]).lower() == "audiodg.exe":
                    self.audiodg.append(process)
            except (psutil.NoSuchProcess, psutil.AccessDenied):
                continue

    @staticmethod
    def process_cpu(process) -> float:
        if process is None:
            return 0.0
        try:
            return float(process.cpu_percent(interval=None))
        except (psutil.NoSuchProcess, psutil.AccessDenied):
            return 0.0

    def sample(self) -> dict[str, float]:
        if not self.available:
            return {}
        times = psutil.cpu_times_percent(interval=None)
        audiodg_cpu = sum(self.process_cpu(process) for process in self.audiodg)
        return {
            "total_cpu": float(psutil.cpu_percent(interval=None)),
            "probe_process_cpu": self.process_cpu(self.probe),
            "audiodg_cpu": float(audiodg_cpu),
            "system_process_cpu": self.process_cpu(self.system),
            "interrupt_cpu": float(getattr(times, "interrupt", 0.0)),
            "dpc_cpu": float(getattr(times, "dpc", 0.0)),
        }


def collect_idle_cpu(monitor: CpuMonitor, seconds: float) -> list[dict[str, float]]:
    rows: list[dict[str, float]] = []
    deadline = time.monotonic() + seconds
    while time.monotonic() < deadline:
        time.sleep(0.100)
        sample = monitor.sample()
        if sample:
            rows.append(sample)
    return rows


def add_cpu_deltas(
    active: dict[str, dict[str, float]], baseline: dict[str, dict[str, float]]
) -> None:
    for key, values in list(active.items()):
        if key == "sample_count" or key not in baseline:
            continue
        values["avg_delta"] = float(values["avg"] - baseline[key]["avg"])
        values["p95_delta"] = float(values["p95"] - baseline[key]["p95"])


def cpu_gate(args: argparse.Namespace, cpu: dict[str, dict[str, float]]) -> list[str]:
    errors: list[str] = []

    def check(key: str, field: str, maximum: float) -> None:
        value = float(cpu.get(key, {}).get(field, float("inf")))
        if value > maximum:
            errors.append(f"{key}.{field}={value:.3f} > {maximum:.3f}")

    check("probe_process_cpu", "avg", args.max_probe_cpu_avg)
    check("audiodg_cpu", "avg", args.max_audiodg_cpu_avg)
    check("system_process_cpu", "avg_delta", args.max_system_cpu_delta_avg)
    check("total_cpu", "avg_delta", args.max_total_cpu_delta_avg)
    check("dpc_cpu", "avg", args.max_dpc_cpu_avg)
    check("dpc_cpu", "p95", args.max_dpc_cpu_p95)
    check("interrupt_cpu", "avg", args.max_interrupt_cpu_avg)
    check("interrupt_cpu", "p95", args.max_interrupt_cpu_p95)
    return errors


def run_direction(
    args: argparse.Namespace,
    direction: str,
    device: int,
    monitor: CpuMonitor,
    baseline_cpu: dict[str, dict[str, float]],
) -> dict:
    state = {"callbacks": 0, "frames": 0, "statuses": [], "last_callback": time.monotonic()}

    def input_callback(indata, frames, timing, status):
        del indata, timing
        if status:
            state["statuses"].append(str(status))
        state["callbacks"] += 1
        state["frames"] += frames
        state["last_callback"] = time.monotonic()

    def output_callback(outdata, frames, timing, status):
        del timing
        outdata.fill(0.0)
        if status:
            state["statuses"].append(str(status))
        state["callbacks"] += 1
        state["frames"] += frames
        state["last_callback"] = time.monotonic()

    stream_type = sd.InputStream if direction == "capture" else sd.OutputStream
    callback = input_callback if direction == "capture" else output_callback
    with stream_type(
        device=device,
        channels=8,
        samplerate=args.rate,
        blocksize=args.blocksize,
        dtype="float32",
        latency="high",
        extra_settings=sd.WasapiSettings(exclusive=True),
        callback=callback,
    ):
        started = time.monotonic()
        deadline = started + args.seconds
        watchdog = False
        cpu_rows: list[dict[str, float]] = []
        while time.monotonic() < deadline:
            if time.monotonic() - state["last_callback"] > 5.0:
                watchdog = True
                break
            time.sleep(0.100)
            sample = monitor.sample()
            if sample:
                cpu_rows.append(sample)
        elapsed = time.monotonic() - started

    cpu = summarize_cpu(cpu_rows)
    add_cpu_deltas(cpu, baseline_cpu)
    cpu_errors = cpu_gate(args, cpu) if monitor.available else ["psutil unavailable"]

    return {
        "direction": direction,
        "device": device,
        "elapsed_seconds": elapsed,
        "callbacks": state["callbacks"],
        "frames": state["frames"],
        "frames_per_second": state["frames"] / elapsed if elapsed else 0.0,
        "callbacks_per_second": state["callbacks"] / elapsed if elapsed else 0.0,
        "status_events": state["statuses"],
        "watchdog_expired": watchdog,
        "cpu": cpu,
        "cpu_errors": cpu_errors,
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--seconds", type=float, default=8.0)
    parser.add_argument("--rate", type=int, default=48000)
    parser.add_argument("--blocksize", type=int, default=512)
    parser.add_argument(
        "--directions",
        choices=("render", "capture", "render,capture"),
        default="render,capture",
    )
    parser.add_argument("--ctl", default="")
    parser.add_argument("--baseline-seconds", type=float, default=3.0)
    parser.add_argument("--max-probe-cpu-avg", type=float, default=20.0)
    parser.add_argument("--max-audiodg-cpu-avg", type=float, default=20.0)
    parser.add_argument("--max-system-cpu-delta-avg", type=float, default=10.0)
    parser.add_argument("--max-total-cpu-delta-avg", type=float, default=20.0)
    parser.add_argument("--max-dpc-cpu-avg", type=float, default=3.0)
    parser.add_argument("--max-dpc-cpu-p95", type=float, default=8.0)
    parser.add_argument("--max-interrupt-cpu-avg", type=float, default=3.0)
    parser.add_argument("--max-interrupt-cpu-p95", type=float, default=8.0)
    parser.add_argument("--out-dir", required=True)
    args = parser.parse_args()
    if (
        args.seconds <= 0
        or args.baseline_seconds <= 0
        or args.rate not in (44100, 48000)
        or args.blocksize <= 0
    ):
        parser.error("invalid cadence arguments")

    capture = find_device(
        "input", "Audio 8 DJ (8ch In) (Audio 8 DJ)", "Windows WASAPI", 8
    )
    render = find_device(
        "output", "Audio 8 DJ (8ch Out) (Audio 8 DJ)", "Windows WASAPI", 8
    )
    monitor = CpuMonitor()
    baseline_rows = collect_idle_cpu(monitor, args.baseline_seconds)
    baseline_cpu = summarize_cpu(baseline_rows)
    results = []
    for direction in args.directions.split(","):
        device = render if direction == "render" else capture
        results.append(
            run_direction(args, direction, device, monitor, baseline_cpu)
        )
        if args.ctl:
            completed = subprocess.run(
                [args.ctl, "diagnostics"],
                check=False,
                capture_output=True,
                text=True,
                timeout=10.0,
            )
            (Path(args.out_dir) / f"driver-diagnostics-after-{direction}.txt").write_text(
                completed.stdout + completed.stderr,
                encoding="utf-8",
            )
            results[-1]["diagnostics_exit"] = int(completed.returncode)
    report = {
        "rate": args.rate,
        "blocksize": args.blocksize,
        "seconds": args.seconds,
        "directions": args.directions,
        "baseline_seconds": args.baseline_seconds,
        "cpu_monitor_available": monitor.available,
        "baseline_cpu": baseline_cpu,
        "results": results,
        "passed": all(
            not result["watchdog_expired"]
            and not result["status_events"]
            and result["frames_per_second"] >= args.rate * 0.98
            and not result["cpu_errors"]
            for result in results
        ),
    }
    out_dir = Path(args.out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)
    write_json(out_dir / "cadence-summary.json", report)
    print(json.dumps(report, indent=2))
    return 0 if report["passed"] else 2


if __name__ == "__main__":
    raise SystemExit(main())
