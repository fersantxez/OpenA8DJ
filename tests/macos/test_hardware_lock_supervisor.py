#!/usr/bin/env python3
"""Process-level tests for the lifecycle hardware-lock supervisor."""
import importlib.util
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import time
import unittest

ROOT = Path(__file__).resolve().parents[2]
SUPERVISOR = ROOT / "scripts/hardware-lock-supervisor.py"


def invoke(lock_root, *command, timeout=60):
    return subprocess.run(
        [sys.executable, str(SUPERVISOR), "--lock-root", str(lock_root),
         "--purpose", "offline-test", "--", *command],
        capture_output=True, text=True, timeout=timeout,
    )


class HardwareLockSupervisorTests(unittest.TestCase):
    def test_lock_contention(self):
        with tempfile.TemporaryDirectory(prefix="opena8dj-lock-") as td:
            root = Path(td) / "gate.lock"
            ready = Path(td) / "ready"
            command = f"touch '{ready}'; sleep 10"
            first = subprocess.Popen([sys.executable, str(SUPERVISOR), "--lock-root", str(root),
                                      "--", "/bin/sh", "-c", command],
                                     stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            deadline = time.monotonic() + 60
            while not ready.exists() and time.monotonic() < deadline:
                time.sleep(0.02)
            self.assertTrue(ready.exists(), "first actor did not enter while owning the lock")
            second = invoke(root, "/bin/true", timeout=60)
            self.assertEqual(second.returncode, 75, f"stdout={second.stdout} stderr={second.stderr}")
            self.assertIn("audio_gate_lock=BUSY", second.stdout)
            first.wait(timeout=60)

    def test_fake_inheritance(self):
        with tempfile.TemporaryDirectory(prefix="opena8dj-lock-") as td:
            root = Path(td) / "gate.lock"
            env = os.environ.copy()
            env.update(OPENA8DJ_LOCK_TOKEN="fake", OPENA8DJ_LOCK_FD="999", OPENA8DJ_LOCK_ROOT=str(root))
            result = subprocess.run([sys.executable, str(SUPERVISOR), "--verify-inherited",
                                     "--lock-root", str(root)], env=env,
                                    capture_output=True, text=True, timeout=60)
            self.assertEqual(result.returncode, 1)
            self.assertIn("audio_gate_lock_inherited=FAIL", result.stdout)

    def test_pid_and_boot_identity(self):
        with tempfile.TemporaryDirectory(prefix="opena8dj-lock-") as td:
            root = Path(td) / "gate.lock"
            root.mkdir()
            (root / "owner").write_text(f"pid={os.getpid()}\ntoken=stale\nboot_uuid=old-boot\n")
            child = (
                "import json,pathlib,sys; "
                "v={}; "
                "[v.update([line.split('=',1)]) for line in pathlib.Path(sys.argv[1]).read_text().splitlines()]; "
                "print(json.dumps(v))"
            )
            result = invoke(root, sys.executable, "-c", child, str(root / "owner"))
            self.assertEqual(result.returncode, 0, result.stderr)
            owner = json.loads(result.stdout)
            self.assertNotEqual(owner["token"], "stale")
            self.assertNotEqual(owner["boot_uuid"], "old-boot")
            self.assertTrue(owner["process_birth"])

    def test_old_owner_cleanup(self):
        spec = importlib.util.spec_from_file_location("hardware_lock_supervisor", SUPERVISOR)
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        with tempfile.TemporaryDirectory(prefix="opena8dj-lock-") as td:
            root = Path(td) / "gate.lock"
            root.mkdir()
            (root / "owner").write_text("token=current\npid=1\n")
            module.cleanup_owner(root, "older-operation")
            self.assertEqual(module.metadata(root).get("token"), "current")
            module.cleanup_owner(root, "current")
            self.assertFalse((root / "owner").exists())

    def test_installer_lease_denied_without_inherited_lock(self):
        with tempfile.TemporaryDirectory(prefix="opena8dj-lock-") as td:
            root = Path(td) / "gate.lock"
            root.mkdir()
            (root / "owner").write_text(
                f"pid={os.getpid()}\ntoken=matching-token\nboot_uuid=unknown\nprocess_birth=not-a-lease\n")
            lock_path = Path(str(root) + ".flock")
            lock_path.touch()
            fd = os.open(lock_path, os.O_RDWR)
            try:
                env = os.environ.copy()
                env.update(OPENA8DJ_LOCK_TOKEN="matching-token", OPENA8DJ_LOCK_FD=str(fd),
                           OPENA8DJ_LOCK_ROOT=str(root))
                result = subprocess.run([sys.executable, str(SUPERVISOR), "--verify-inherited",
                                         "--lock-root", str(root)], env=env, pass_fds=(fd,),
                                        capture_output=True, text=True, timeout=60)
            finally:
                os.close(fd)
            self.assertEqual(result.returncode, 1)

    def test_pid_reuse_rejected_even_with_inherited_lock_descriptor(self):
        with tempfile.TemporaryDirectory(prefix="opena8dj-lock-reuse-") as td:
            spec = importlib.util.spec_from_file_location("hardware_lock_supervisor", SUPERVISOR)
            module = importlib.util.module_from_spec(spec)
            spec.loader.exec_module(module)
            root = Path(td) / "gate.lock"
            root.mkdir()
            lock_path = Path(str(root) + ".flock")
            lock_path.touch()
            fd = os.open(lock_path, os.O_RDWR)
            import fcntl
            fcntl.flock(fd, fcntl.LOCK_EX | fcntl.LOCK_NB)
            (root / "owner").write_text(
                f"pid={os.getpid()}\ntoken=token-A\nboot_uuid={module.boot_id()}\n"
                "process_birth=Mon Jan  1 00:00:00 2001\n")
            env = os.environ.copy()
            env.update(OPENA8DJ_LOCK_TOKEN="token-A", OPENA8DJ_LOCK_FD=str(fd),
                       OPENA8DJ_LOCK_ROOT=str(root))
            try:
                result = subprocess.run([sys.executable, str(SUPERVISOR), "--verify-inherited",
                                         "--lock-root", str(root)], env=env, pass_fds=(fd,),
                                        capture_output=True, text=True, timeout=60)
            finally:
                os.close(fd)
            self.assertEqual(result.returncode, 1)
            self.assertIn("audio_gate_lock_inherited=FAIL", result.stdout)

    def test_mutating_entrypoint_uses_supervisor_before_work(self):
        with tempfile.TemporaryDirectory(prefix="opena8dj-lock-entrypoint-") as td:
            root = Path(td) / "hardware-gate.lock"
            env = os.environ.copy()
            env["AUDIO_GATE_LOCK_ROOT"] = str(root)
            result = subprocess.run([str(ROOT / "scripts/test-hal-candidate-safety"), "--help"],
                                    env=env, capture_output=True, text=True, timeout=60)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertIn("usage:", result.stdout)
            self.assertTrue(Path(str(root) + ".flock").is_file())
            self.assertFalse((root / "owner").exists(), "supervisor must clear only its own metadata")

    def test_supervisor_dies_with_child(self):
        with tempfile.TemporaryDirectory(prefix="opena8dj-lock-") as td:
            root = Path(td) / "gate.lock"
            ready = Path(td) / "child-ready"
            child_code = (
                "import pathlib,sys,time,os; pathlib.Path(sys.argv[1]).write_text(str(os.getpid())); time.sleep(10)"
            )
            supervisor = subprocess.Popen(
                [sys.executable, str(SUPERVISOR), "--lock-root", str(root), "--",
                 sys.executable, "-c", child_code, str(ready)],
                stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
            )
            deadline = time.monotonic() + 60
            while not ready.exists() and time.monotonic() < deadline:
                time.sleep(0.02)
            self.assertTrue(ready.exists())
            child_pid = int(ready.read_text())
            supervisor.kill()
            supervisor.wait(timeout=60)
            contender = invoke(root, "/usr/bin/true", timeout=60)
            self.assertEqual(contender.returncode, 75)
            os.kill(child_pid, 0)
            deadline = time.monotonic() + 60
            while time.monotonic() < deadline:
                try:
                    os.kill(child_pid, 0)
                except ProcessLookupError:
                    break
                time.sleep(0.05)
            else:
                self.fail("inherited lock holder did not exit")
            after = invoke(root, "/usr/bin/true", timeout=60)
            self.assertEqual(after.returncode, 0, after.stderr)


if __name__ == "__main__":
    unittest.main()
