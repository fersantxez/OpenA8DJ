#!/usr/bin/env python3
"""Offline exact-restoration tests for the timecode smoke gate helper."""

import os
from pathlib import Path
import stat
import subprocess
import tempfile


def check(condition, message):
    if not condition:
        raise AssertionError(message)


def run():
    repo = Path(__file__).resolve().parents[1]
    helper = repo / "scripts/timecode-control-restore"
    with tempfile.TemporaryDirectory(prefix="a8-timecode-restore-", dir="/tmp") as tmp:
        base = Path(tmp)
        state = base / "state.json"
        baseline = base / "baseline.json"
        restored = base / "restored.json"
        log_dir = base / "logs"
        mock = base / "mock-control"
        baseline.write_text('{"profile":"custom","decode":true,"route":[3,2,1,0]}\\n')
        state.write_text('{"profile":"temporary"}\\n')
        mock.write_text(
            "#!/usr/bin/env bash\n"
            "set -euo pipefail\n"
            f"state={state!s}\n"
            "case \"$1\" in\n"
            "  import-config) cp \"$2\" \"$state\" ;;\n"
            "  export-config) cp \"$state\" \"$2\" ;;\n"
            "  *) exit 2 ;;\n"
            "esac\n"
        )
        mock.chmod(mock.stat().st_mode | stat.S_IXUSR)
        result = subprocess.run(
            [str(helper), str(mock), str(baseline), str(restored), str(log_dir)],
            check=False,
            env={**os.environ},
        )
        check(result.returncode == 0, "exact restoration helper failed")
        check(state.read_bytes() == baseline.read_bytes(), "state was not restored")
        check(restored.read_bytes() == baseline.read_bytes(), "read-back differs")

        mock.write_text(mock.read_text().replace(
            'cp "$state" "$2"', 'printf "{\\"corrupt\\":true}\\\\n" >"$2"'
        ))
        result = subprocess.run(
            [str(helper), str(mock), str(baseline), str(restored), str(log_dir)],
            check=False,
            env={**os.environ},
        )
        check(result.returncode != 0, "read-back mismatch was accepted")
    print("timecode exact control restoration: PASS")


if __name__ == "__main__":
    run()
