#!/usr/bin/env python3
"""Summarize numeric quality metrics from an OpenA8DJ test run directory."""

import argparse
import re
from pathlib import Path


KEY_VALUE = re.compile(r"^([A-Za-z0-9_.-]+)=(.*)$")


def read_key_values(path):
    values = {}
    if not path.exists():
        return values
    for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        match = KEY_VALUE.match(line.strip())
        if match:
            values[match.group(1)] = match.group(2).strip()
    return values


def float_value(values, key, default=0.0):
    try:
        return float(values.get(key, default))
    except (TypeError, ValueError):
        return default


def int_value(values, key, default=0):
    try:
        return int(float(values.get(key, default)))
    except (TypeError, ValueError):
        return default


def parse_stream_stats(path):
    metrics = {}
    if not path.exists():
        return metrics
    text = path.read_text(encoding="utf-8", errors="replace")

    patterns = {
        "sample_rate": r"sample-rate:\s+([0-9.]+)",
        "output_ring_frames": r"output-ring:\s+([0-9]+)",
        "output_ring_target": r"output-ring:\s+[0-9]+\s+/\s+target\s+([0-9]+)",
        "output_byte_position": r"output-byte-position:\s+([0-9]+)",
        "playback_in_flight": r"playback-queue:\s+in-flight=([0-9]+)",
        "playback_queue_target": r"playback-queue:\s+in-flight=[0-9]+\s+/\s+target\s+([0-9]+)",
        "capture_transfers": r"capture:\s+transfers=([0-9]+)",
        "capture_tx": r"capture:\s+transfers=[0-9]+\s+tx=([0-9]+)",
        "capture_bytes": r"capture:\s+transfers=[0-9]+\s+tx=[0-9]+\s+bytes=([0-9]+)",
        "capture_failed": r"capture:.*failed=([0-9]+)",
        "capture_short": r"capture:.*short=([0-9]+)",
        "capture_filtered": r"capture:.*filtered=([0-9]+)",
        "capture_qfail": r"capture:.*qfail=([0-9]+)",
        "playback_transfers": r"playback:\s+transfers=([0-9]+)",
        "playback_tx": r"playback:\s+transfers=[0-9]+\s+tx=([0-9]+)",
        "playback_bytes": r"playback:\s+transfers=[0-9]+\s+tx=[0-9]+\s+bytes=([0-9]+)",
        "playback_failed": r"playback:.*failed=([0-9]+)",
        "playback_short": r"playback:.*short=([0-9]+)",
        "playback_qfail": r"playback:.*qfail=([0-9]+)",
        "output_written": r"output:\s+written=([0-9]+)",
        "output_read": r"output:.*read=([0-9]+)",
        "output_underruns": r"output:.*underruns=([0-9]+)",
        "output_active_underruns": r"output:.*active-underruns=([0-9]+)",
        "output_startup_silence": r"output:.*startup-silence=([0-9]+)",
        "output_overruns": r"output:.*overruns=([0-9]+)",
        "output_elastic_drops": r"output:.*elastic-drops=([0-9]+)",
        "output_elastic_replays": r"output:.*elastic-replays=([0-9]+)",
        "output_timeline_resets": r"output:.*timeline-resets=([0-9]+)",
        "output_peak": r"output-level:\s+peak=([0-9.]+)",
        "output_near_clip": r"output-level:.*near-clip=([0-9]+)",
        "output_clipped": r"output-level:.*clipped=([0-9]+)",
        "schedule_resets": r"scheduling:.*resets=([0-9]+)",
        "schedule_too_old": r"scheduling:.*too-old=([0-9]+)",
        "schedule_too_new": r"scheduling:.*too-new=([0-9]+)",
        "schedule_out_of_window": r"scheduling:.*out-of-window=([0-9]+)",
        "schedule_fallbacks": r"scheduling:.*fallbacks=([0-9]+)",
        "mode2_input_check_errors": r"mode2:.*input-check-errors=([0-9]+)",
        "mode2_output_panic_flags": r"mode2:.*output-panic-flags=([0-9]+)",
    }
    for key, pattern in patterns.items():
        match = re.search(pattern, text)
        if match:
            metrics[key] = match.group(1)

    capture_tx = float_value(metrics, "capture_tx")
    capture_failed = float_value(metrics, "capture_failed")
    capture_filtered = float_value(metrics, "capture_filtered")
    playback_tx = float_value(metrics, "playback_tx")
    playback_failed = float_value(metrics, "playback_failed")
    if capture_tx + capture_failed > 0:
        metrics["capture_failure_rate"] = f"{capture_failed / (capture_tx + capture_failed):.6f}"
    if capture_tx + capture_filtered > 0:
        metrics["capture_filter_rate"] = f"{capture_filtered / (capture_tx + capture_filtered):.6f}"
    if playback_tx + playback_failed > 0:
        metrics["playback_failure_rate"] = f"{playback_failed / (playback_tx + playback_failed):.6f}"
    return metrics


def verdict_from_pass(values):
    if not values:
        return "missing"
    return "pass" if values.get("pass") == "1" else "fail"


def verdict_usb(values):
    if not values:
        return "missing"
    errors = int_value(values, "usb_check_errors")
    panic = int_value(values, "usb_panic_flags")
    score = float_value(values, "usb_alignment_score")
    left_snr = float_value(values, "usb_left_snr_db")
    right_snr = float_value(values, "usb_right_snr_db")
    if errors == 0 and panic == 0 and score >= 0.995 and min(left_snr, right_snr) >= 50.0:
        return "pass"
    return "fail"


def verdict_stream(values):
    if not values:
        return "missing"
    hard_fail_keys = (
        "playback_failed",
        "playback_qfail",
        "output_active_underruns",
        "output_overruns",
        "output_elastic_drops",
        "output_elastic_replays",
        "output_timeline_resets",
        "output_clipped",
        "mode2_input_check_errors",
        "mode2_output_panic_flags",
    )
    if any(int_value(values, key) != 0 for key in hard_fail_keys):
        return "fail"
    return "pass"


def emit_section(lines, title, verdict, metrics):
    lines.append(f"[{title}] verdict={verdict}")
    for key in sorted(metrics):
        lines.append(f"{title}.{key}={metrics[key]}")


def metric(values, key):
    return values.get(key, "missing")


def markdown_report(run_dir,
                    gate_status,
                    gate_reason,
                    release_gate,
                    variant,
                    stream_verdict,
                    internal_verdict,
                    usb_verdict,
                    analog_verdict,
                    input_raw_verdict,
                    stream,
                    internal,
                    usb,
                    analog,
                    input_raw):
    return "\n".join([
        "# OpenA8DJ Quality Pass",
        "",
        f"- Run dir: `{run_dir}`",
        f"- Change: {metric(variant, 'change_note')}",
        f"- Version: {metric(variant, 'bundle_version')} build {metric(variant, 'bundle_build')}",
        f"- HAL sha256: `{metric(variant, 'hal_sha256')}`",
        f"- Git: `{metric(variant, 'git_head')}` dirty={metric(variant, 'git_dirty')}",
        f"- Variant: output_byte_order={metric(variant, 'output_byte_order')}, rate={metric(variant, 'rate')}, buffer={metric(variant, 'buffer')}, pair={metric(variant, 'pair')}",
        f"- Gate: {gate_status or 'UNKNOWN'} {('(' + gate_reason + ')') if gate_reason else ''}",
        f"- Release gate: {release_gate}",
        "",
        "## Verdicts",
        "",
        f"- Stream: {stream_verdict}",
        f"- Internal consumed audio: {internal_verdict}",
        f"- USB output bytes: {usb_verdict}",
        f"- Analog loopback: {analog_verdict}",
        f"- Raw input correlation: {input_raw_verdict}",
        "",
        "## Headline Metrics",
        "",
        f"- Stream output peak: {metric(stream, 'output_peak')}",
        f"- Active underruns: {metric(stream, 'output_active_underruns')}",
        f"- Playback failed transactions: {metric(stream, 'playback_failed')}",
        f"- Playback failure rate: {metric(stream, 'playback_failure_rate')}",
        f"- Capture failure rate: {metric(stream, 'capture_failure_rate')}",
        f"- Mode-2 input check errors: {metric(stream, 'mode2_input_check_errors')}",
        f"- Mode-2 output panic flags: {metric(stream, 'mode2_output_panic_flags')}",
        f"- Internal min SNR dB: {metric(internal, 'min_snr_db')}",
        f"- Internal min correlation: {metric(internal, 'min_correlation')}",
        f"- USB output alignment score: {metric(usb, 'usb_alignment_score')}",
        f"- USB output check errors: {metric(usb, 'usb_check_errors')}",
        f"- USB output panic flags: {metric(usb, 'usb_panic_flags')}",
        f"- Analog loopback min SNR dB: {metric(analog, 'min_snr_db')}",
        f"- Analog loopback min correlation: {metric(analog, 'min_correlation')}",
        f"- Input raw alignment score: {metric(input_raw, 'usb_alignment_score')}",
        "",
        "## Artifacts",
        "",
        "- `quality-summary.txt`: full key-value metric dump",
        "- `stream-stats-live.txt`: live stream stats samples during playback",
        "- `internal-consumed-analysis.txt`: reference vs consumed f32",
        "- `usb-raw-analysis.txt`: decoded outgoing USB bytes",
        "- `loopback-analysis.txt`: decoded input/analog-loopback analysis",
        "- `input-raw-auto-analysis.txt`: raw input USB auto-decode attempt",
        "",
    ])


def main():
    parser = argparse.ArgumentParser(description="Summarize an OpenA8DJ quality run.")
    parser.add_argument("run_dir")
    parser.add_argument("--status", default="")
    parser.add_argument("--reason", default="")
    parser.add_argument("--write", action="store_true")
    args = parser.parse_args()

    run_dir = Path(args.run_dir)
    variant = read_key_values(run_dir / "build-variant.txt")
    internal = read_key_values(run_dir / "internal-consumed-analysis.txt")
    usb = read_key_values(run_dir / "usb-raw-analysis.txt")
    analog = read_key_values(run_dir / "loopback-analysis.txt")
    input_raw = read_key_values(run_dir / "input-raw-auto-analysis.txt")
    stream = parse_stream_stats(run_dir / "stream-stats-after.txt")

    stream_verdict = verdict_stream(stream)
    internal_verdict = verdict_from_pass(internal)
    usb_verdict = verdict_usb(usb)
    analog_verdict = verdict_from_pass(analog)
    input_raw_verdict = verdict_usb(input_raw)

    blockers = []
    if stream_verdict != "pass":
        blockers.append("stream")
    if internal_verdict != "pass":
        blockers.append("internal")
    if usb_verdict != "pass":
        blockers.append("usb_output")
    if analog_verdict != "pass":
        blockers.append("analog_loopback")

    lines = []
    lines.append(f"run_dir={run_dir}")
    if args.status:
        lines.append(f"gate_status={args.status}")
    if args.reason:
        lines.append(f"gate_reason={args.reason}")
    release_gate = "fail:" + ",".join(blockers) if blockers else "pass"
    lines.append(f"headline.release_gate={release_gate}")
    lines.append(f"headline.change_note={variant.get('change_note', 'missing')}")
    lines.append(f"headline.bundle_version={variant.get('bundle_version', 'missing')}")
    lines.append(f"headline.bundle_build={variant.get('bundle_build', 'missing')}")
    lines.append(f"headline.stream={stream_verdict}")
    lines.append(f"headline.internal={internal_verdict}")
    lines.append(f"headline.usb_output={usb_verdict}")
    lines.append(f"headline.analog_loopback={analog_verdict}")
    lines.append(f"headline.input_raw={input_raw_verdict}")
    lines.append(f"headline.stream.output_peak={stream.get('output_peak', 'missing')}")
    lines.append(f"headline.stream.active_underruns={stream.get('output_active_underruns', 'missing')}")
    lines.append(f"headline.stream.playback_failed={stream.get('playback_failed', 'missing')}")
    lines.append(f"headline.stream.playback_failure_rate={stream.get('playback_failure_rate', 'missing')}")
    lines.append(f"headline.stream.capture_failure_rate={stream.get('capture_failure_rate', 'missing')}")
    lines.append(f"headline.internal.min_snr_db={internal.get('min_snr_db', 'missing')}")
    lines.append(f"headline.internal.min_correlation={internal.get('min_correlation', 'missing')}")
    lines.append(f"headline.usb_output.alignment_score={usb.get('usb_alignment_score', 'missing')}")
    lines.append(f"headline.usb_output.check_errors={usb.get('usb_check_errors', 'missing')}")
    lines.append(f"headline.usb_output.panic_flags={usb.get('usb_panic_flags', 'missing')}")
    lines.append(f"headline.analog_loopback.min_snr_db={analog.get('min_snr_db', 'missing')}")
    lines.append(f"headline.analog_loopback.min_correlation={analog.get('min_correlation', 'missing')}")
    emit_section(lines, "variant", "info", variant)
    emit_section(lines, "stream", stream_verdict, stream)
    emit_section(lines, "internal", internal_verdict, internal)
    emit_section(lines, "usb_output", usb_verdict, usb)
    emit_section(lines, "analog_loopback", analog_verdict, analog)
    emit_section(lines, "input_raw", input_raw_verdict, input_raw)
    if blockers:
        lines.append(f"release_gate=fail:{','.join(blockers)}")
    else:
        lines.append("release_gate=pass")

    output = "\n".join(lines) + "\n"
    if args.write:
        (run_dir / "quality-summary.txt").write_text(output, encoding="utf-8")
        report = markdown_report(run_dir,
                                 args.status,
                                 args.reason,
                                 release_gate,
                                 variant,
                                 stream_verdict,
                                 internal_verdict,
                                 usb_verdict,
                                 analog_verdict,
                                 input_raw_verdict,
                                 stream,
                                 internal,
                                 usb,
                                 analog,
                                 input_raw)
        (run_dir / "pass-report.md").write_text(report, encoding="utf-8")
    print(output, end="")


if __name__ == "__main__":
    main()
