#!/usr/bin/env python3
"""Sample Core Audio related CPU over aligned monotonic windows.

The evaluator is deliberately observational. It identifies a process by PID,
birth time and boot session, so PID reuse cannot merge two processes.
"""
import argparse
import ctypes
import json
import math
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile
import time
from datetime import datetime, timezone

ROOT = Path(__file__).resolve().parents[1]
WATCHED = {
    "coreaudiod": ("coreaudiod",),
    "opena8dj_driver": ("core audio driver (opena8dj.driver)",),
    "airplay_xpc": ("airplayxpchelper",),
    "mediaremoted": ("mediaremoted",),
    "audioaccessoryd": ("audioaccessoryd",),
    "audiomxd": ("audiomxd",),
    "systemsoundserverd": ("systemsoundserverd",),
    "controlcenter": ("controlcenter",),
    "continuity_capture": ("continuitycaptureagent",),
    "callservicesd": ("callservicesd",),
    "perf_power": ("perfpowerservices",),
    "corespeechd": ("corespeechd",),
    "heard": ("/heard",),
    "avconferenced": ("avconferenced",),
    "spotify": ("spotify",),
    "ni_hardware_agent": ("nihardwareagent",),
}


def boot_uuid():
    result = subprocess.run(["/usr/sbin/sysctl", "-n", "kern.bootsessionuuid"],
                            capture_output=True, text=True, timeout=3)
    if result.returncode:
        raise RuntimeError("could not read boot session UUID")
    return result.stdout.strip()


def parse_ps(text, boot):
    rows = []
    for line in text.splitlines():
        match = re.match(r"^\s*(\d+)\s+(.{24})\s+(\S+)\s+(.*)$", line)
        if not match:
            continue
        pid, birth, cpu_time, command = match.groups()
        lower = command.lower()
        labels = [label for label, needles in WATCHED.items()
                  if any(needle in lower for needle in needles)]
        if labels:
            rows.append({"pid": int(pid), "birth": birth.strip(), "boot_uuid": boot,
                         "cpu_pct": 0.0, "cpu_time_seconds": parse_cpu_time(cpu_time),
                         "labels": labels, "command": command})
    return rows


def parse_cpu_time(value):
    """Parse ps cumulative CPU time: MM:SS.ss, HH:MM:SS.ss, or D-HH:MM:SS."""
    days = 0
    if "-" in value:
        day_text, value = value.split("-", 1)
        days = int(day_text)
    fields = value.split(":")
    if len(fields) == 1:
        seconds = float(fields[0])
    elif len(fields) == 2:
        seconds = int(fields[0]) * 60 + float(fields[1])
    elif len(fields) == 3:
        seconds = int(fields[0]) * 3600 + int(fields[1]) * 60 + float(fields[2])
    else:
        raise ValueError(f"invalid ps CPU time: {value}")
    return days * 86400 + seconds


def read_host_cpu_ticks():
    """Read cumulative host CPU ticks through Mach without spawning top."""
    try:
        system = ctypes.CDLL("/usr/lib/libSystem.B.dylib")
        system.mach_host_self.restype = ctypes.c_uint32
        system.host_statistics.argtypes = [ctypes.c_uint32, ctypes.c_int,
                                           ctypes.POINTER(ctypes.c_uint32),
                                           ctypes.POINTER(ctypes.c_uint32)]
        system.host_statistics.restype = ctypes.c_int
        host = system.mach_host_self()
        ticks = (ctypes.c_uint32 * 4)()
        count = ctypes.c_uint32(4)
        status = system.host_statistics(host, 3, ticks, ctypes.byref(count))
        try:
            task = ctypes.c_uint32.in_dll(system, "mach_task_self_").value
            system.mach_port_deallocate.argtypes = [ctypes.c_uint32, ctypes.c_uint32]
            system.mach_port_deallocate.restype = ctypes.c_int
            system.mach_port_deallocate(task, host)
        except (ValueError, OSError):
            pass
        if status != 0 or count.value < 3:
            raise RuntimeError(f"host_statistics failed: {status}")
        return tuple(int(ticks[index]) for index in range(4))
    except (AttributeError, OSError) as exc:
        raise RuntimeError(f"Mach CPU statistics unavailable: {exc}") from exc


def cpu_idle_percent(previous, current):
    delta = [((new - old) & 0xFFFFFFFF) for old, new in zip(previous, current)]
    total = sum(delta)
    if total == 0:
        raise RuntimeError("Mach CPU counters did not advance")
    return 100.0 * delta[2] / total


def evaluate_samples(samples, thresholds, idle_samples=None):
    """Evaluate equally spaced snapshots; values are per-core CPU percentages."""
    identities = set()
    totals = []
    labels_by_sample = []
    coreaudiod_samples = []
    host_samples = []
    idle_samples = list(idle_samples or [])
    for sample in samples:
        if isinstance(sample, dict):
            rows = sample["processes"]
            if sample.get("global_idle_pct") is not None:
                idle_samples.append(float(sample["global_idle_pct"]))
        else:
            rows = sample
        seen = set()
        grouped = {name: 0.0 for name in WATCHED}
        sample_total = 0.0
        for row in rows:
            ident = (row["pid"], row["birth"], row["boot_uuid"])
            if ident in seen:
                continue
            seen.add(ident)
            identities.add(ident)
            sample_total += row["cpu_pct"]
            for label in row["labels"]:
                grouped[label] += row["cpu_pct"]
        labels_by_sample.append(grouped)
        totals.append(sample_total)
        coreaudiod_samples.append(grouped["coreaudiod"])
        host_samples.append(grouped["opena8dj_driver"])
    if not samples:
        raise ValueError("at least one sample is required")
    mean = lambda values: sum(values) / len(values)
    result = {
        "status": "PASS",
        "sample_count": len(samples),
        "unique_process_identities": len(identities),
        "mean_watched_total_cpu_pct": round(mean(totals), 2),
        "mean_coreaudiod_cpu_pct": round(mean(coreaudiod_samples), 2),
        "mean_opena8dj_host_cpu_pct": round(mean(host_samples), 2),
        "mean_process_cpu_pct": {
            label: round(mean([row[label] for row in labels_by_sample]), 2)
            for label in WATCHED
        },
        "thresholds": thresholds,
        "violations": [],
    }
    if result["mean_watched_total_cpu_pct"] > thresholds["total"]:
        result["violations"].append("watched_total_cpu")
    if result["mean_coreaudiod_cpu_pct"] > thresholds["core_audio"]:
        result["violations"].append("coreaudiod_cpu")
    if result["mean_opena8dj_host_cpu_pct"] > thresholds["core_audio"]:
        result["violations"].append("opena8dj_host_cpu")
    if idle_samples:
        result["mean_global_idle_pct"] = round(mean(idle_samples), 2)
        if "global_idle" in thresholds and result["mean_global_idle_pct"] < thresholds["global_idle"]:
            result["violations"].append("global_idle_below_threshold")
    elif "global_idle" in thresholds:
        result["status"] = "INFRA_ERROR"
        result["violations"].append("global_idle_sample_missing")
    if result["violations"]:
        if result["status"] != "INFRA_ERROR":
            result["status"] = "FAIL"
    return result


def collect_sample(ps_bin, boot):
    result = subprocess.run([ps_bin, "-axo", "pid=,lstart=,time=,comm="],
                            capture_output=True, text=True, timeout=5)
    if result.returncode:
        raise RuntimeError(f"ps exited {result.returncode}: {result.stderr.strip()}")
    return parse_ps(result.stdout, boot)


def apply_task_cpu_deltas(rows, previous, counters, elapsed_seconds):
    """Replace ps lifetime averages with per-interval CPU percentages."""
    current = {}
    for row in rows:
        identity = (row["pid"], row["birth"], row["boot_uuid"])
        value = counters.get(identity)
        if value is None:
            continue
        current[identity] = value
        old = previous.get(identity)
        if old is None:
            row["cpu_pct"] = 0.0
            continue
        delta = value - old
        if delta < 0:
            raise RuntimeError(f"task CPU counter moved backwards for {identity}")
        row["cpu_pct"] = 100.0 * delta / (elapsed_seconds * 1_000_000_000)
    return [row for row in rows if (row["pid"], row["birth"], row["boot_uuid"]) in current], current


def collect_task_cpu_sample(ps_bin, boot, previous, elapsed_seconds):
    rows = collect_sample(ps_bin, boot)
    counters = {
        (row["pid"], row["birth"], row["boot_uuid"]):
        int(round(row["cpu_time_seconds"] * 1_000_000_000))
        for row in rows
    }
    return apply_task_cpu_deltas(rows, previous, counters, elapsed_seconds)


def enumerate_window(binary, timeout, evidence_dir, window_index):
    """Enumerate Core Audio once at a window boundary under a child deadline."""
    if not binary:
        return {"status": "NOT_CONFIGURED", "exit_code": None, "timed_out": False}
    directory = Path(evidence_dir) if evidence_dir else Path(tempfile.mkdtemp(prefix="opena8dj-window-enum-"))
    directory.mkdir(parents=True, exist_ok=True)
    stdout_path = directory / f"window-{window_index + 1:02d}-audio-list.stdout"
    stderr_path = directory / f"window-{window_index + 1:02d}-audio-list.stderr"
    started = time.monotonic_ns()
    runner = ROOT / "scripts/run-child-with-timeout.py"
    try:
        result = subprocess.run(
            [sys.executable, str(runner), str(timeout), str(stdout_path), str(stderr_path), binary],
            capture_output=True, text=True, timeout=timeout + 3,
        )
        code = result.returncode
    except subprocess.TimeoutExpired:
        code = 124
    status = ("PASS" if code == 0 else "HUNG" if code == 124 else
              "SIGNALED" if code >= 128 else "INFRA_ERROR" if code in (126, 127) else "FAIL")
    return {"status": status, "exit_code": code if code < 128 else None,
            "signal": code - 128 if code >= 128 else None, "timed_out": code == 124,
            "duration_ns": time.monotonic_ns() - started,
            "stdout_path": str(stdout_path), "stderr_path": str(stderr_path)}


def main(argv=None):
    parser = argparse.ArgumentParser()
    parser.add_argument("--threshold", type=float, default=25.0)
    parser.add_argument("--total-threshold", type=float, default=120.0)
    parser.add_argument("--wait", type=float, default=0)
    parser.add_argument("--windows", type=int, default=3)
    parser.add_argument("--window-seconds", type=float, default=10)
    parser.add_argument("--sample-interval", type=float, default=1)
    parser.add_argument("--min-idle-pct", type=float, default=20)
    parser.add_argument("--audio-list-bin")
    parser.add_argument("--enumeration-timeout", type=float, default=5)
    parser.add_argument("--evidence-dir")
    parser.add_argument("--reset", action="store_true")
    args = parser.parse_args(argv)
    if args.reset:
        print(json.dumps({"status": "INFRA_ERROR", "reason": "mutating_reset_requires_transaction_manager"}))
        return 2
    if args.windows < 1 or args.window_seconds <= 0 or args.sample_interval <= 0:
        parser.error("windows and intervals must be positive")
    if args.wait:
        time.sleep(args.wait)
    try:
        boot = boot_uuid()
        ps_bin = os.environ.get("OPENA8DJ_HEALTH_PS_BIN", "/bin/ps")
        per_window = max(1, math.ceil(args.window_seconds / args.sample_interval))
        samples = []
        idle_samples = []
        windows = []
        previous_process_cpu = {}
        # Prime cumulative process CPU counters so the first interval has a real delta.
        baseline_rows = collect_sample(ps_bin, boot)
        for row in baseline_rows:
            identity = (row["pid"], row["birth"], row["boot_uuid"])
            previous_process_cpu[identity] = int(round(row["cpu_time_seconds"] * 1_000_000_000))
        previous_monotonic = time.monotonic()
        for _ in range(args.windows):
            bucket = []
            window_idle = []
            wall_started = datetime.now(timezone.utc).isoformat()
            monotonic_started = time.monotonic_ns()
            previous_ticks = read_host_cpu_ticks()
            for _index in range(per_window):
                time.sleep(args.sample_interval)
                now = time.monotonic()
                elapsed = now - previous_monotonic
                if elapsed <= 0:
                    raise RuntimeError("monotonic process sample interval did not advance")
                processes, previous_process_cpu = collect_task_cpu_sample(
                    ps_bin, boot, previous_process_cpu, elapsed)
                current_ticks = read_host_cpu_ticks()
                bucket.append(processes)
                window_idle.append(cpu_idle_percent(previous_ticks, current_ticks))
                previous_ticks = current_ticks
                previous_monotonic = now
            idle_samples.extend(window_idle)
            thresholds = {"core_audio": args.threshold, "total": args.total_threshold,
                          "global_idle": args.min_idle_pct}
            evaluation = evaluate_samples(bucket, thresholds, window_idle)
            evaluation["started_at"] = wall_started
            evaluation["ended_at"] = datetime.now(timezone.utc).isoformat()
            evaluation["monotonic_started_ns"] = monotonic_started
            evaluation["monotonic_ended_ns"] = time.monotonic_ns()
            evaluation["boot_uuid"] = boot
            enumeration = enumerate_window(args.audio_list_bin, args.enumeration_timeout,
                                           args.evidence_dir, len(windows))
            evaluation["enumeration"] = enumeration
            if enumeration["status"] not in ("PASS", "NOT_CONFIGURED"):
                evaluation["violations"].append("core_audio_enumeration")
                evaluation["status"] = ("INFRA_ERROR" if enumeration["status"] in
                                         ("HUNG", "SIGNALED", "INFRA_ERROR") else "FAIL")
            windows.append(evaluation)
            samples.extend(bucket)
        result = evaluate_samples(samples, {"core_audio": args.threshold,
                                            "total": args.total_threshold,
                                            "global_idle": args.min_idle_pct}, idle_samples)
        result["windows"] = windows
        failed_windows = [index for index, window in enumerate(windows) if window["status"] != "PASS"]
        if failed_windows:
            window_infra = any(windows[index]["status"] == "INFRA_ERROR" for index in failed_windows)
            if window_infra:
                result["status"] = "INFRA_ERROR"
            elif result["status"] != "INFRA_ERROR":
                result["status"] = "FAIL"
            result["violations"] = sorted(set(result["violations"] + ["window_threshold_or_enumeration_failed"]))
        result["failed_window_indexes"] = failed_windows
        result["boot_uuid"] = boot
        print(json.dumps(result, sort_keys=True))
        print(f"audio_stack_health={result['status']}")
        return 0 if result["status"] == "PASS" else 2 if result["status"] == "INFRA_ERROR" else 1
    except (OSError, RuntimeError, subprocess.TimeoutExpired) as exc:
        print(json.dumps({"status": "INFRA_ERROR", "reason": str(exc)}, sort_keys=True))
        print("audio_stack_health=INFRA_ERROR")
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
