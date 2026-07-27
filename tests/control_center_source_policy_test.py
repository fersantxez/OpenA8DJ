#!/usr/bin/env python3
import argparse
import pathlib
import re
import sys


def fail(message):
    raise AssertionError(message)


def check(condition, message):
    if not condition:
        fail(message)


def run(repo):
    source_dir = repo / "macos" / "OpenA8DJControlCenter"
    sources = sorted(source_dir.glob("*.swift"))
    check(sources, "control center Swift sources missing")
    text = "\n".join(path.read_text(encoding="utf-8") for path in sources)

    forbidden = {
        "/tmp/opena8dj-control.sock": "private socket",
        "/usr/local/bin": "installed-tool fallback",
        "URLSession": "network API",
        "Network.framework": "network framework",
        "import Network": "network framework import",
        "WebView": "web view",
        "WKWebView": "web view",
        "IOUSB": "direct USB API",
        "import IOKit": "direct IOKit access",
        "NSAppleScript": "script execution",
        "/bin/sh": "shell",
        "/bin/bash": "shell",
        "launchPath": "legacy process launch",
        "export-config": "prohibited legacy command",
        "import-config": "prohibited legacy command",
        "apply-preset": "prohibited legacy command",
    }
    for token, purpose in forbidden.items():
        check(token not in text, f"{purpose} token present: {token}")

    runner = (source_dir / "ProcessRunner.swift").read_text(encoding="utf-8")
    models = (source_dir / "Models.swift").read_text(encoding="utf-8")
    store = (source_dir / "ControlCenterStore.swift").read_text(encoding="utf-8")
    views = (source_dir / "Views.swift").read_text(encoding="utf-8")

    required_runner = [
        "Bundle.main.resourceURL",
        ".isRegularFileKey",
        ".isSymbolicLinkKey",
        ".isExecutableKey",
        "process.executableURL",
        "process.arguments = operation.arguments",
        'environment["OPENA8DJ_CONTROL_NO_WAKE"] = "1"',
        "limit: 512 * 1024",
        "limit: 32 * 1024",
        "process.terminate()",
        "process.interrupt()",
        "SIGKILL",
        "process.waitUntilExit()",
    ]
    for token in required_runner:
        check(token in runner, f"runner safety policy missing: {token}")

    check("standardOutput = stdoutPipe" in runner and
          "standardError = stderrPipe" in runner,
          "stdout/stderr are not separate")
    check(runner.count("readabilityHandler") >= 4,
          "both streams are not installed and cleared")
    check("while active" in runner and "Task.checkCancellation()" in runner,
          "serialized acquire is not cancellation-aware")

    exact_vectors = [
        '["api", "version"]',
        '["api", "profiles"]',
        '["api", "profile"]',
        '["api", "driver-modes"]',
        '["api", "driver-mode"]',
        '["api", "stats"]',
        '["api", "loopback", "get"]',
        '["usb-quality", "--json", "--interval-ms", "1000", "--count", "2"]',
        '["api", "driver-mode", "arm", "timecode-optimized", "--input-pairs", "A,B"]',
        '["api", "driver-mode", "disarm", "timecode-optimized"]',
    ]
    for vector in exact_vectors:
        check(vector in models, f"typed operation vector missing: {vector}")

    check("PendingConfirmation" in store and
          "application-readable virtual input for this session" in store,
          "loopback privacy confirmation missing")
    check("Experimental — Unverified" in store and
          "Experimental — Unverified" in views,
          "Vintage warning is not literal")
    check("await coordinator.cancel()" in store,
          "mutations do not suspend polling")
    check("readBackMatches" in store and "compensate(" in store,
          "mutation read-back/compensation missing")
    check(not re.search(r"Timer\.scheduledTimer|DispatchSourceTimer", text),
          "unbounded background timer present")

    makefile = (repo / "Makefile").read_text(encoding="utf-8")
    for token in [
        "control-center-offline-test:",
        "control-center-smoke-test:",
        "opena8dj-hardware-profiler",
        "hardware-profiler-known-issues-v1.json",
        "codesign --verify --deep --strict",
    ]:
        check(token in makefile, f"build/bundle policy missing: {token}")

    for script_name in ["preinstall", "postinstall",
                        "uninstall-opena8dj-control-surfaces.sh"]:
        script = (repo / "resources" / "control-surfaces-pkg" /
                  "scripts" / script_name).read_text(encoding="utf-8")
        check("opena8dj-control" in script, f"{script_name} misses control tool")
        check("opena8dj-hardware-profiler" in script,
              f"{script_name} misses profiler tool")
        check("hardware-profiler-known-issues-v1.json" in script,
              f"{script_name} misses catalog")
        check("/Library/Application Support/OpenA8DJ" in script,
              f"{script_name} does not scope catalog path")
        check('rm -rf "/Library/Application Support/OpenA8DJ"' not in script,
              f"{script_name} broadens Application Support removal")

    print("control center source policy tests: PASS")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo", type=pathlib.Path, required=True)
    args = parser.parse_args()
    try:
        run(args.repo.resolve())
    except AssertionError as error:
        print(f"control center source policy tests: FAIL: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
