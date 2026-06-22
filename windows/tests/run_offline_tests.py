#!/usr/bin/env python3
import os
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]


def run(argv):
    print("+ " + " ".join(str(arg) for arg in argv), flush=True)
    subprocess.run(argv, cwd=ROOT, check=True)


def main():
    compiler = os.environ.get("CC") or shutil.which("clang") or shutil.which("cc")
    if compiler is None:
        raise SystemExit("FAIL: no C compiler found; set CC or install clang")

    run([sys.executable, "windows/tests/validate_windows_surface_contract.py"])

    with tempfile.TemporaryDirectory(prefix="opena8dj-win-offline-") as temp_dir:
        test_binary = Path(temp_dir) / "audio_engine_contract"
        run([
            compiler,
            "-std=c11",
            "-Wall",
            "-Wextra",
            "-Werror",
            "-Iwindows/audio",
            "windows/audio/OpenA8DJAudioEngine.c",
            "windows/tests/audio_engine_contract_test.c",
            "-o",
            str(test_binary),
        ])
        run([str(test_binary)])

    print("PASS: OpenA8DJ Windows offline tests")


if __name__ == "__main__":
    main()
