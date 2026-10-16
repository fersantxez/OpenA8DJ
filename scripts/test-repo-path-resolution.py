#!/usr/bin/env python3
"""Prove a configured tool ignores a decoy checkout in its working directory."""
import subprocess
import sys
import tempfile
from pathlib import Path

if len(sys.argv) != 3:
    raise SystemExit("usage: test-repo-path-resolution.py PROBE EXPECTED_ROOT")
probe, expected = Path(sys.argv[1]).resolve(), Path(sys.argv[2]).resolve()
with tempfile.TemporaryDirectory(prefix="opena8dj-decoy-") as temp:
    decoy = Path(temp)
    (decoy / "CMakeLists.txt").write_text("# decoy checkout\n")
    (decoy / "tools").mkdir()
    (decoy / "tools/decoy-only.txt").write_text("must never be used\n")
    found = subprocess.check_output([str(probe)], cwd=decoy, text=True).strip()
    if Path(found).resolve() != expected:
        raise SystemExit(f"FAIL: probe resolved {found}, expected {expected}")
    print(f"PASS: configured root={found}; decoy ignored")
