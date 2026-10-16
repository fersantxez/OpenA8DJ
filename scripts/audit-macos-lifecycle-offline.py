#!/usr/bin/env python3
"""Reproduce known lifecycle defects without using USB, Core Audio I/O or live IPC.

Exit 0 means three targeted regressions pass; it is not full-driver certification.
The harness includes production code and substitutes a fake USB device only for
the partial-open fixture. Its unlink shim protects the installed IPC socket.
"""
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tempfile


def main():
    root = Path(__file__).resolve().parents[1]
    if sys.platform != "darwin":
        raise SystemExit("This offline harness requires the macOS SDK.")
    report_dir = root / "local-analysis" / "macos-lifecycle-audit"
    report_dir.mkdir(parents=True, exist_ok=True)
    report = {
        "purpose": "known-defect reproduction, not a stability pass",
        "source_commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], cwd=root, text=True).strip(),
        "usb_source_sha256": hashlib.sha256(
            (root / "src/hal/OpenA8DJUSB.m").read_bytes()).hexdigest(),
        "hardware_access": False,
        "live_ipc_access": False,
    }
    with tempfile.TemporaryDirectory(prefix="opena8dj-lifecycle-") as temp:
        binary = str(Path(temp) / "audit")
        command = ["xcrun", "clang", "-fobjc-arc", "-O1", "-g",
                   "-Wno-deprecated-declarations",
                   str(root / "tools/audit_macos_lifecycle.m"),
                   "-framework", "Foundation", "-framework", "CoreAudio",
                   "-framework", "CoreMIDI", "-framework", "IOKit",
                   "-framework", "IOUSBHost", "-o", binary]
        built = subprocess.run(command, cwd=root, capture_output=True, text=True, timeout=60)
        (report_dir / "compile.txt").write_text(built.stdout + built.stderr)
        built.check_returncode()
        result = subprocess.run([binary], cwd=root, capture_output=True, text=True, timeout=10)
        report.update(returncode=result.returncode, stdout=result.stdout, stderr=result.stderr)
        report["three_regressions_pass"] = (
            result.returncode == 0 and "PASS: 3 lifecycle regressions fixed" in result.stdout)
    (report_dir / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(result.stdout, end="")
    print(f"Evidence: {report_dir / 'report.json'}")
    return 0 if report["three_regressions_pass"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
