#!/usr/bin/env python3
"""Hold a stable advisory lock across a hardware-mutating child process."""
import argparse
import fcntl
import json
import os
from pathlib import Path
import signal
import stat
import subprocess
import sys
import tempfile
import time
import uuid


def boot_id():
    try:
        result = subprocess.run(["/usr/sbin/sysctl", "-n", "kern.bootsessionuuid"],
                                capture_output=True, text=True, timeout=3)
        if result.returncode == 0 and result.stdout.strip():
            return result.stdout.strip()
    except (OSError, subprocess.TimeoutExpired):
        pass
    return "unknown"


def metadata(root):
    result = {}
    path = root / "owner"
    if path.is_file():
        for line in path.read_text(errors="replace").splitlines():
            key, sep, value = line.partition("=")
            if sep:
                result[key] = value
    return result


def process_birth(pid):
    try:
        result = subprocess.run(["/bin/ps", "-p", str(int(pid)), "-o", "lstart="],
                                capture_output=True, text=True, timeout=3)
        return result.stdout.strip() if result.returncode == 0 else ""
    except (OSError, ValueError, subprocess.TimeoutExpired):
        return ""


def write_owner(root, values):
    root.mkdir(parents=True, exist_ok=True)
    fd, name = tempfile.mkstemp(prefix="owner.", dir=root)
    try:
        with os.fdopen(fd, "w") as stream:
            for key, value in values.items():
                stream.write(f"{key}={value}\n")
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(name, root / "owner")
        dir_fd = os.open(root, os.O_RDONLY)
        try:
            os.fsync(dir_fd)
        finally:
            os.close(dir_fd)
    finally:
        try:
            os.unlink(name)
        except FileNotFoundError:
            pass


def verify_inherited_lock(root, token, fd_text):
    if not token or not fd_text:
        return False
    try:
        fd = int(fd_text)
        held = os.fstat(fd)
        expected = os.stat(Path(str(root) + ".flock"))
        if (held.st_dev, held.st_ino) != (expected.st_dev, expected.st_ino):
            return False
        owner = metadata(root)
        if owner.get("token") != token or owner.get("boot_uuid") != boot_id():
            return False
        pid = int(owner.get("pid", ""))
        try:
            os.kill(pid, 0)
        except OSError:
            return False
        if not owner.get("process_birth") or process_birth(pid) != owner.get("process_birth"):
            return False
        # Test from a distinct open-file description. It must conflict with
        # the lock retained by the supervisor's inherited descriptor.
        probe = os.open(Path(str(root) + ".flock"), os.O_RDWR | getattr(os, "O_NOFOLLOW", 0))
        try:
            try:
                fcntl.flock(probe, fcntl.LOCK_EX | fcntl.LOCK_NB)
                fcntl.flock(probe, fcntl.LOCK_UN)
                return False
            except BlockingIOError:
                return True
        finally:
            os.close(probe)
    except (OSError, ValueError):
        return False


def cleanup_owner(root, token):
    current = metadata(root)
    if current.get("token") == token:
        try:
            (root / "owner").unlink()
        except FileNotFoundError:
            pass


def run(args):
    root = Path(args.lock_root).expanduser().resolve()
    root.parent.mkdir(mode=0o700, parents=True, exist_ok=True)
    lock_path = Path(str(root) + ".flock")
    fd = os.open(lock_path, os.O_CREAT | os.O_RDWR | getattr(os, "O_NOFOLLOW", 0), 0o600)
    try:
        if not stat.S_ISREG(os.fstat(fd).st_mode):
            print("hardware lock path is not a regular file", file=sys.stderr)
            return 2
        try:
            fcntl.flock(fd, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except BlockingIOError:
            current = metadata(root)
            print("audio_gate_lock=BUSY")
            for key in ("pid", "started_at", "purpose", "token", "boot_uuid"):
                if current.get(key):
                    print(f"{key}={current[key]}")
            return 75

        token = uuid.uuid4().hex
        values = {
            "pid": os.getpid(),
            "owner": "codex",
            "token": token,
            "boot_uuid": boot_id(),
            "process_birth": process_birth(os.getpid()),
            "worktree": args.worktree,
            "purpose": args.purpose,
            "started_at": time.strftime("%Y-%m-%dT%H:%M:%S%z"),
            "actions_authorized": args.actions,
            "forbidden": args.forbidden,
            "evidence": args.evidence,
        }
        write_owner(root, values)
        child_env = os.environ.copy()
        child_env.update({
            "OPENA8DJ_LOCK_ROOT": str(root),
            "OPENA8DJ_LOCK_FILE": str(lock_path),
            "OPENA8DJ_LOCK_FD": str(fd),
            "OPENA8DJ_LOCK_TOKEN": token,
            "OPENA8DJ_SUPERVISOR_ACTIVE": "1",
        })
        command = args.command
        if command and command[0] == "--":
            command = command[1:]
        if not command:
            print("a child command is required", file=sys.stderr)
            cleanup_owner(root, token)
            return 2

        child = subprocess.Popen(command, env=child_env, pass_fds=(fd,), start_new_session=True)
        pending_signal = {"value": None, "at": None}

        def forward(signum, _frame):
            pending_signal["value"] = signum
            pending_signal["at"] = time.monotonic()
            try:
                os.killpg(child.pid, signum)
            except ProcessLookupError:
                pass

        old_handlers = {sig: signal.signal(sig, forward) for sig in (signal.SIGTERM, signal.SIGINT, signal.SIGHUP)}
        try:
            while True:
                try:
                    result = child.wait(timeout=0.1)
                    break
                except subprocess.TimeoutExpired:
                    if pending_signal["at"] is None or time.monotonic() - pending_signal["at"] < 2:
                        continue
                    try:
                        os.killpg(child.pid, signal.SIGKILL)
                    except ProcessLookupError:
                        pass
                    result = child.wait()
                    break
            if pending_signal["value"] is not None and result == 0:
                result = 128 + pending_signal["value"]
            return result
        finally:
            for sig, handler in old_handlers.items():
                signal.signal(sig, handler)
            cleanup_owner(root, token)
    finally:
        os.close(fd)


def main(argv=None):
    parser = argparse.ArgumentParser()
    parser.add_argument("--verify-inherited", action="store_true")
    parser.add_argument("--lock-root", default=os.environ.get(
        "AUDIO_GATE_LOCK_ROOT", str(Path.home() / ".opena8dj/hardware-gate.lock")))
    parser.add_argument("--worktree", default="")
    parser.add_argument("--purpose", default="hardware operation")
    parser.add_argument("--evidence", default="")
    parser.add_argument("--actions", default="")
    parser.add_argument("--forbidden", default="")
    parser.add_argument("command", nargs=argparse.REMAINDER)
    args = parser.parse_args(argv)
    if args.verify_inherited:
        ok = verify_inherited_lock(Path(args.lock_root).expanduser().resolve(),
                                   os.environ.get("OPENA8DJ_LOCK_TOKEN"),
                                   os.environ.get("OPENA8DJ_LOCK_FD"))
        print(f"audio_gate_lock_inherited={'PASS' if ok else 'FAIL'}")
        return 0 if ok else 1
    return run(args)


if __name__ == "__main__":
    raise SystemExit(main())
