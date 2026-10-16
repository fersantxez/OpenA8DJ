#!/usr/bin/env python3
"""Capture repository/build identity without touching Core Audio or USB."""
import hashlib
import json
import os
from pathlib import Path
import platform
import subprocess
import sys
from datetime import datetime, timezone

ROOT = Path(__file__).resolve().parents[1]


def run(*args):
    try:
        result = subprocess.run(args, cwd=ROOT, capture_output=True, text=True, timeout=10)
        return {"exit_code": result.returncode, "stdout": result.stdout.strip(), "stderr": result.stderr.strip()}
    except (OSError, subprocess.TimeoutExpired) as exc:
        return {"error": str(exc)}


def sha256_bytes(value):
    return hashlib.sha256(value).hexdigest()


def sha256_file(path):
    h = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()


def sha256_tree(path):
    items = [(str(p.relative_to(path)), sha256_file(p))
             for p in sorted(path.rglob("*")) if p.is_file()]
    return sha256_bytes(json.dumps(items, separators=(",", ":")).encode())


def main(argv):
    output = Path(argv[1]) if len(argv) > 1 else ROOT / "local-analysis/lifecycle/20261011-offline/manifest.json"
    if not output.is_absolute():
        output = ROOT / output
    tracked = run("git", "ls-files", "-z")
    tracked_paths = tracked.get("stdout", "").split("\0") if tracked.get("stdout") else []
    untracked = run("git", "ls-files", "--others", "--exclude-standard", "-z")
    untracked_paths = untracked.get("stdout", "").split("\0") if untracked.get("stdout") else []
    relevant = sorted(set(p for p in tracked_paths + untracked_paths if p and (
        p.startswith(("src/hal/", "scripts/", "tests/macos/", "tools/")) or
        p in ("CMakeLists.txt", "Makefile", "resources/OpenA8DJ.driver/Contents/Info.plist")
    )))
    input_hashes = {p: sha256_file(ROOT / p) for p in relevant if (ROOT / p).is_file()}
    try:
        boot = run("sysctl", "-n", "kern.bootsessionuuid")
    except Exception as exc:
        boot = {"error": str(exc)}
    build_config = ROOT / "build/hal-build-config.txt"
    flags_stamp = ROOT / "build/.hal-cflags.stamp"
    bundle = ROOT / "build/OpenA8DJ.driver"
    executable = bundle / "Contents/MacOS/OpenA8DJHAL"
    signature = run("codesign", "-dv", "--verbose=4", str(bundle)) if bundle.exists() else {"error": "bundle_missing"}
    signature_verify = run("codesign", "--verify", "--deep", "--strict", str(bundle)) if bundle.exists() else {"error": "bundle_missing"}
    uuid = run("dwarfdump", "--uuid", str(executable)) if executable.exists() else {"error": "executable_missing"}
    manifest = {
        "schema": "opena8dj.macos-lifecycle-run-manifest.v1",
        "captured_at_utc": datetime.now(timezone.utc).isoformat(),
        "hardware_access": "none",
        "user_reported_device_state": "Audio 8 unplugged",
        "repository": {
            "head": run("git", "rev-parse", "HEAD").get("stdout"),
            "branch": run("git", "branch", "--show-current").get("stdout"),
            "tracked_diff_sha256": sha256_bytes(run("git", "diff", "--binary").get("stdout", "").encode()),
            "staged_diff_sha256": sha256_bytes(run("git", "diff", "--cached", "--binary").get("stdout", "").encode()),
            "source_inputs_sha256": sha256_bytes(json.dumps(input_hashes, sort_keys=True).encode()),
            "source_inputs": input_hashes,
            "working_tree_status": run("git", "status", "--short").get("stdout", "").splitlines(),
        },
        "toolchain": {
            "os": platform.platform(),
            "xcode": run("xcodebuild", "-version"),
            "compiler": run("xcrun", "clang", "--version"),
            "macos_sdk": run("xcrun", "--sdk", "macosx", "--show-sdk-version"),
            "boot_session_uuid": boot.get("stdout"),
        },
        "build": {
            "makefile_sha256": sha256_file(ROOT / "Makefile"),
            "effective_flags_stamp_sha256": sha256_file(flags_stamp) if flags_stamp.exists() else None,
            "effective_flags": flags_stamp.read_text(errors="replace").strip() if flags_stamp.exists() else None,
            "legacy_config_file_sha256": sha256_file(build_config) if build_config.exists() else None,
            "legacy_config_version": next((x.partition("=")[2] for x in build_config.read_text(errors="replace").splitlines() if x.startswith("VERSION=")), None) if build_config.exists() else None,
            "info_plist_sha256": sha256_file(ROOT / "resources/OpenA8DJ.driver/Contents/Info.plist"),
            "bundle_sha256": sha256_tree(bundle) if bundle.is_dir() else None,
            "bundle_signature_verification": signature_verify,
            "bundle_signature_details": signature,
            "binary_uuid": uuid,
            "dsym_present": Path(str(executable) + ".dSYM").exists(),
            "bundle_path": str(bundle),
            "bundle_present_at_capture": bundle.exists(),
        },
        "claims": {
            "host_pid_birth": None,
            "module_loaded": None,
            "device_enumerated": None,
            "stream_progress": None,
            "host_healthy": None,
        },
    }
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n")
    print(output)
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
