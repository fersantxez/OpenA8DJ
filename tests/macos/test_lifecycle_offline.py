#!/usr/bin/env python3
"""Offline lifecycle regression tests. All guard subprocesses are fakes."""
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
GUARD = ROOT / "scripts/audio-stack-guard"


def write_executable(path: Path, content: str) -> Path:
    path.write_text(content)
    path.chmod(0o755)
    return path


class LifecycleOfflineTests(unittest.TestCase):
    def run_guard(self, audio_list=None, extra=(), timeout=180, enumeration_timeout=10,
                  health_exit=0, idle_pct=97, min_idle_pct=0,
                  health_sleep=0, health_timeout=40):
        with tempfile.TemporaryDirectory(prefix="opena8dj-guard-test-") as td:
            base = Path(td)
            fake_top = write_executable(base / "top", f"#!/bin/sh\nprintf 'Processes: first\\nCPU usage: 1.0%% user, 2.0%% sys, {idle_pct}%% idle\\nProcesses: second\\nCPU usage: 1.0%% user, 2.0%% sys, {idle_pct}%% idle\\n'\n")
            fake_pgrep = write_executable(base / "pgrep", "#!/bin/sh\nexit 1\n")
            fake_health = write_executable(base / "health", f"#!/bin/sh\nsleep {health_sleep}\necho audio_stack_health={'PASS' if health_exit == 0 else 'FAIL'}\nexit {health_exit}\n")
            listing = audio_list or base / "audio-list"
            if audio_list is None:
                write_executable(listing, "#!/bin/sh\nexit 0\n")
            run_dir = base / "run"
            env = os.environ.copy()
            env.update({
                "OPENA8DJ_GUARD_TOP_BIN": str(fake_top),
                "OPENA8DJ_GUARD_PGREP_BIN": str(fake_pgrep),
                "OPENA8DJ_AUDIO_HEALTH_BIN": str(fake_health),
                "OPENA8DJ_AUDIO_LIST_BIN": str(listing),
                "OPENA8DJ_GUARD_HEALTH_TIMEOUT": str(health_timeout),
            })
            result = subprocess.run(
                [str(GUARD), "--run-dir", str(run_dir), "--min-idle-pct", str(min_idle_pct),
                 "--enumeration-timeout", str(enumeration_timeout), *extra],
                env=env, capture_output=True, text=True, timeout=timeout,
            )
            status_file = run_dir / "audio-list-status.json"
            status = json.loads(status_file.read_text()) if status_file.exists() else None
            return result, status

    def test_R00_manifest_identity(self):
        policy = json.loads((ROOT / "tests/macos/lifecycle_test_policy.json").read_text())
        self.assertEqual(policy["schema"], "opena8dj.macos-lifecycle-test-policy.v1")
        self.assertEqual(policy["transactions"]["normal_deadline_seconds"], 90)
        spec = importlib.util.spec_from_file_location("lifecycle_manifest", ROOT / "scripts/verify-lifecycle-manifest.py")
        verifier = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(verifier)
        actual = json.loads((ROOT / "local-analysis/lifecycle/20261012-offline/manifest.json").read_text())
        self.assertEqual(verifier.validate(actual, ROOT), [])
        if actual["build"]["bundle_sha256"]:
            self.assertEqual(verifier.validate(actual, ROOT, require_bundle=True), [])
        else:
            self.assertIn("candidate_bundle_hash_missing", verifier.validate(actual, ROOT, require_bundle=True))

        with tempfile.TemporaryDirectory(prefix="opena8dj-manifest-test-") as td:
            fixture_root = Path(td)
            source = fixture_root / "driver.c"
            source.write_text("driver-source-v1")
            source_hash = hashlib.sha256(source.read_bytes()).hexdigest()
            inputs = {"driver.c": source_hash}
            manifest = {
                "schema": "opena8dj.macos-lifecycle-run-manifest.v1",
                "repository": {
                    "source_inputs": inputs,
                    "source_inputs_sha256": hashlib.sha256(json.dumps(inputs, sort_keys=True).encode()).hexdigest(),
                },
                "build": {"bundle_sha256": None},
            }
            self.assertEqual(verifier.validate(manifest, fixture_root), [])
            source.write_text("driver-source-v2")
            self.assertTrue(any(x.startswith("source_hash_mismatch") for x in verifier.validate(manifest, fixture_root)))

    def test_R00_evidence_inventory(self):
        inv = json.loads((ROOT / "tools/test_inventory.json").read_text())
        self.assertEqual(inv["test_count"], len(inv["tests"]))
        local = [x for x in inv["tests"] if x["classification"] == "local_evidence"]
        self.assertEqual(len(local), 16)
        self.assertTrue((ROOT / "local-analysis/lifecycle/20261010T173942/shutdown-stall-report.txt").exists())
        build_config = (ROOT / "build/hal-build-config.txt")
        self.assertTrue(build_config.exists())
        self.assertIn("0.3.135", build_config.read_text())

    def test_R01_exit_codes(self):
        cases = [(0, "PASS", 0), (7, "FAIL", 7)]
        for code, wanted, return_code in cases:
            with self.subTest(code=code):
                script = Path(tempfile.mkdtemp()) / "audio-list"
                write_executable(script, f"#!/bin/sh\nexit {code}\n")
                result, status = self.run_guard(script)
                self.assertEqual(status["status"], wanted)
                self.assertEqual(status["exit_code"], return_code)
                self.assertEqual(result.returncode, 0 if code == 0 else 1)

        script = Path(tempfile.mkdtemp()) / "audio-list"
        write_executable(script, "#!/bin/sh\nexec sleep 10\n")
        result, status = self.run_guard(script, timeout=180, enumeration_timeout=2)
        self.assertEqual(status["status"], "HUNG")
        self.assertEqual(status["exit_code"], 124)
        self.assertEqual(result.returncode, 1)

        script = Path(tempfile.mkdtemp()) / "audio-list"
        write_executable(script, "#!/bin/sh\nkill -TERM $$\n")
        result, status = self.run_guard(script)
        self.assertEqual(status["status"], "SIGNALED")
        self.assertEqual(status["signal"], 15)
        self.assertEqual(result.returncode, 1)

        result, status = self.run_guard(Path("/does/not/exist"))
        self.assertEqual(status["status"], "INFRA_ERROR")
        self.assertEqual(result.returncode, 2)

    def test_R01_no_mutation(self):
        for option in ("--recover", "--unload-opena8dj", "--force-unload-opena8dj"):
            result, status = self.run_guard(extra=(option,))
            self.assertEqual(result.returncode, 2)
            self.assertIn("transaction_manager", result.stdout)
            self.assertIsNone(status)

    def test_R01_environment_busy_classification(self):
        result, _ = self.run_guard(health_exit=1, idle_pct=90)
        self.assertEqual(result.returncode, 3)
        self.assertIn("audio_stack_guard=ENVIRONMENT_BUSY", result.stdout)
        result, _ = self.run_guard(health_exit=0, idle_pct=5, min_idle_pct=20)
        self.assertEqual(result.returncode, 3)
        self.assertIn("audio_stack_guard=ENVIRONMENT_BUSY", result.stdout)

    def test_R01_probe_timeout(self):
        with tempfile.TemporaryDirectory(prefix="opena8dj-timeout-test-") as td:
            base = Path(td)
            stdout = base / "stdout"
            stderr = base / "stderr"
            marker = base / "term-received"
            command = "trap 'echo term > \"$1\"; exit 0' TERM; while :; do sleep 0.05; done"
            result = subprocess.run(
                ["/usr/bin/python3", str(ROOT / "scripts/run-child-with-timeout.py"),
                 "5", str(stdout), str(stderr), "/bin/sh", "-c", command, "timeout-child", str(marker)],
                capture_output=True, text=True, timeout=60,
            )
            self.assertEqual(result.returncode, 124)
            self.assertTrue(marker.exists(), "the owned child process group should receive TERM")
            self.assertTrue(stdout.exists())
            self.assertTrue(stderr.exists())

    def test_R01_health_probe_timeout(self):
        result, _ = self.run_guard(health_sleep=10, health_timeout=1)
        self.assertEqual(result.returncode, 2)
        self.assertIn('"status":"HUNG"', result.stdout)
        self.assertIn("audio_stack_guard=INFRA_ERROR", result.stdout)

    def test_R01_health_windows_and_pid_reuse(self):
        spec = importlib.util.spec_from_file_location("audio_stack_health", ROOT / "scripts/audio-stack-health.py")
        health = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(health)
        thresholds = {"core_audio": 25, "total": 120}
        def row(pid, birth, cpu, label="coreaudiod"):
            return {"pid": pid, "birth": birth, "boot_uuid": "boot-A", "cpu_pct": cpu,
                    "labels": [label], "command": label}
        # Reusing a PID creates a new identity; duplicate observations of the
        # same process in one sample are counted once.
        mixed = health.evaluate_samples([[row(42, "birth-1", 2), row(42, "birth-1", 2)],
                                         [row(42, "birth-2", 30)]], thresholds)
        self.assertEqual(mixed["unique_process_identities"], 2)
        self.assertEqual(mixed["mean_coreaudiod_cpu_pct"], 16.0)
        self.assertEqual(mixed["status"], "PASS")
        # A brief spike averages away; sustained load fails the aligned window.
        spike = health.evaluate_samples([[row(7, "same", 70)], [row(7, "same", 0)],
                                         [row(7, "same", 0)]], thresholds)
        self.assertEqual(spike["status"], "PASS")
        sustained = health.evaluate_samples([[row(7, "same", 40)] for _ in range(5)], thresholds)
        self.assertEqual(sustained["status"], "FAIL")
        self.assertIn("coreaudiod_cpu", sustained["violations"])
        idle_spike = health.evaluate_samples([
            {"processes": [], "global_idle_pct": 5},
            {"processes": [], "global_idle_pct": 50},
            {"processes": [], "global_idle_pct": 50},
        ], {"core_audio": 25, "total": 120, "global_idle": 20})
        self.assertEqual(idle_spike["status"], "PASS")
        self.assertAlmostEqual(health.parse_cpu_time("0:12.54"), 12.54)
        self.assertAlmostEqual(health.parse_cpu_time("1:02:03.5"), 3723.5)
        self.assertAlmostEqual(health.parse_cpu_time("2-01:00:00"), 176400.0)
        sample_line = f"  42 {'Thu Apr  1 00:00:27 1976':24} 0:12.54 /usr/sbin/coreaudiod"
        parsed = health.parse_ps(sample_line, "boot-A")
        self.assertEqual(len(parsed), 1)
        self.assertEqual(parsed[0]["cpu_time_seconds"], 12.54)

        previous_identity = (42, "birth-1", "boot-A")
        new_identity = (42, "birth-2", "boot-A")
        rows, next_counters = health.apply_task_cpu_deltas(
            [row(42, "birth-1", 99), row(42, "birth-2", 99)],
            {previous_identity: 1_000_000_000},
            {previous_identity: 1_200_000_000, new_identity: 7_000_000},
            1.0,
        )
        self.assertEqual([r["cpu_pct"] for r in rows], [20.0, 0.0])
        self.assertEqual(next_counters, {previous_identity: 1_200_000_000,
                                         new_identity: 7_000_000})
        with tempfile.TemporaryDirectory(prefix="opena8dj-window-enum-test-") as td:
            base = Path(td)
            ok = write_executable(base / "audio-list-ok", "#!/bin/sh\necho devices\nexit 0\n")
            failed = write_executable(base / "audio-list-failed", "#!/bin/sh\nexit 7\n")
            slow = write_executable(base / "audio-list-slow", "#!/bin/sh\nexec sleep 10\n")
            evidence = base / "evidence"
            self.assertEqual(health.enumerate_window(str(ok), 2, evidence, 0)["status"], "PASS")
            self.assertEqual(health.enumerate_window(str(failed), 5, evidence, 1)["status"], "FAIL")
            hung = health.enumerate_window(str(slow), 1, evidence, 2)
            self.assertEqual(hung["status"], "HUNG")
            self.assertTrue(Path(hung["stdout_path"]).exists())
            self.assertTrue(Path(hung["stderr_path"]).exists())
        idle_busy = health.evaluate_samples([
            {"processes": [], "global_idle_pct": 10} for _ in range(4)
        ], {"core_audio": 25, "total": 120, "global_idle": 20})
        self.assertEqual(idle_busy["status"], "FAIL")
        self.assertIn("global_idle_below_threshold", idle_busy["violations"])


if __name__ == "__main__":
    unittest.main()
