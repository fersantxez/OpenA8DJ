#!/usr/bin/env python3
"""Failure injection for the filesystem-only lifecycle transaction core."""
import importlib.util
import json
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location("lifecycle_transaction", ROOT / "scripts/lifecycle-transaction.py")
TX = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(TX)


def bundle(path, value):
    path.mkdir(parents=True)
    (path / "Contents").mkdir()
    (path / "Contents" / "payload").write_text(value)
    return path


class LifecycleTransactionTests(unittest.TestCase):
    def test_fault_every_precommit_phase_restores_baseline(self):
        for phase in ("preflight", "staged", "backup_moved", "replaced", "validated"):
            with self.subTest(phase=phase), tempfile.TemporaryDirectory(prefix="opena8dj-tx-") as td:
                root = Path(td)
                candidate = bundle(root / "candidate.driver", "candidate")
                active = bundle(root / "active.driver", "baseline")
                state = root / "state"
                baseline_hash = TX.tree_hash(active)
                with self.assertRaisesRegex(RuntimeError, "injected failure"):
                    TX.install(candidate, active, state, fault=phase)
                self.assertEqual(TX.tree_hash(active), baseline_hash)
                self.assertEqual(TX.recover(state)["result"], "ALREADY_ROLLED_BACK")
                journal = __import__("json").loads((state / "transaction.json").read_text())
                self.assertEqual(journal["phase"], "rollback_validated")
                rejected = list(state.glob("rejected-*"))
                self.assertEqual(bool(rejected), phase in ("replaced", "validated"))

    def test_install_commits_exact_candidate_and_retains_baseline(self):
        with tempfile.TemporaryDirectory(prefix="opena8dj-tx-") as td:
            root = Path(td)
            candidate = bundle(root / "candidate.driver", "candidate")
            active = bundle(root / "active.driver", "baseline")
            state = root / "state"
            original_hash = TX.tree_hash(active)
            result = TX.install(candidate, active, state)
            self.assertEqual(result["phase"], "committed")
            self.assertEqual(TX.tree_hash(active), TX.tree_hash(candidate))
            self.assertTrue(any(TX.tree_hash(path) == original_hash for path in state.glob("baseline-*")))

    def test_invalid_and_symlink_paths_fail_before_mutation(self):
        with tempfile.TemporaryDirectory(prefix="opena8dj-tx-") as td:
            root = Path(td)
            candidate = bundle(root / "candidate.driver", "candidate")
            active = bundle(root / "active.driver", "baseline")
            state = root / "state"
            baseline_hash = TX.tree_hash(active)
            link = root / "linked.driver"
            link.symlink_to(candidate, target_is_directory=True)
            with self.assertRaisesRegex(ValueError, "symlinks"):
                TX.install(link, active, state)
            with self.assertRaisesRegex(ValueError, "outside"):
                TX.install(candidate, active, active / "inside-state")
            self.assertEqual(TX.tree_hash(active), baseline_hash)
            self.assertFalse(state.exists())

    def test_resume_after_interrupted_rollback(self):
        for checkpoint in ("rollback_rejected", "rollback_restored", "rollback_stage_removed"):
            with self.subTest(checkpoint=checkpoint), tempfile.TemporaryDirectory(prefix="opena8dj-tx-resume-") as td:
                root = Path(td).resolve()
                candidate_source = bundle(root / "candidate-source.driver", "candidate")
                candidate_hash = TX.tree_hash(candidate_source)
                active = bundle(root / "active.driver", "candidate")
                backup = bundle(root / "state" / "baseline-op", "baseline")
                baseline_hash = TX.tree_hash(backup)
                state = root / "state"
                stage = active.parent / ".active.driver.stage-op"
                bundle(stage, "partial-stage")
                journal = {
                    "schema": TX.SCHEMA,
                    "operation_id": "op",
                    "phase": "replaced",
                    "candidate": str(candidate_source),
                    "candidate_sha256": candidate_hash,
                    "active": str(active),
                    "original_sha256": baseline_hash,
                    "original_present": True,
                    "stage": str(stage),
                    "backup": str(backup),
                }
                TX.durable_write(state / "transaction.json", journal)
                with self.assertRaisesRegex(RuntimeError, "injected recovery interruption"):
                    TX.recover(state, fault=checkpoint)
                recovered = TX.recover(state)
                self.assertEqual(recovered["result"], "ROLLED_BACK")
                self.assertEqual(TX.tree_hash(active), baseline_hash)
                self.assertEqual(json.loads((state / "transaction.json").read_text())["phase"],
                                 "rollback_validated")

    def test_physical_installer_fails_closed(self):
        with tempfile.TemporaryDirectory(prefix="opena8dj-installer-closed-") as td:
            lock = Path(td) / "hardware-gate.lock"
            env = os.environ.copy()
            env["AUDIO_GATE_LOCK_ROOT"] = str(lock)
            result = subprocess.run([str(ROOT / "scripts/install-hal-lifecycle-transaction")],
                                    env=env, capture_output=True, text=True, timeout=60)
            self.assertEqual(result.returncode, 2, result.stderr)
            self.assertIn("hal_lifecycle_install=BLOCKED", result.stdout)
            self.assertIn("privileged_transaction_backend", result.stdout)
            self.assertFalse((lock / "owner").exists())


if __name__ == "__main__":
    unittest.main()
