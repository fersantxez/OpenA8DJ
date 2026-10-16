#!/usr/bin/env python3
"""Run one owned process group with a hard monotonic deadline."""
import os
import signal
import subprocess
import sys


def main(argv):
    if len(argv) < 5:
        print("usage: run-child-with-timeout.py SECONDS STDOUT STDERR COMMAND [ARGS...]", file=sys.stderr)
        return 2
    timeout = float(argv[1])
    if timeout <= 0:
        print("timeout must be positive", file=sys.stderr)
        return 2
    with open(argv[2], "wb") as stdout, open(argv[3], "wb") as stderr:
        try:
            proc = subprocess.Popen(argv[4:], stdout=stdout, stderr=stderr, start_new_session=True)
        except OSError as exc:
            print(f"could not start child: {exc}", file=sys.stderr)
            return 126
        try:
            return_code = proc.wait(timeout=timeout)
        except subprocess.TimeoutExpired:
            try:
                os.killpg(proc.pid, signal.SIGTERM)
            except ProcessLookupError:
                pass
            try:
                proc.wait(timeout=1.0)
            except subprocess.TimeoutExpired:
                try:
                    os.killpg(proc.pid, signal.SIGKILL)
                except ProcessLookupError:
                    pass
                proc.wait()
            return 124
    return 128 + -return_code if return_code < 0 else return_code


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
