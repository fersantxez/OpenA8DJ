#!/usr/bin/env python3
"""Crash-recoverable, filesystem-only HAL bundle transaction primitives.

This module has no default system paths and never invokes sudo/CoreAudio. The
transaction uses an explicit fixture root in tests; privileged integration is
performed only by the separately supervised installer after its offline gate.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import sys
import time
import uuid

SCHEMA = "opena8dj.lifecycle-transaction.v1"
PHASES = ("preflight", "staged", "backup_moved", "replaced", "validated", "committed")


def tree_hash(path):
    if not path.exists():
        return None
    if path.is_symlink() or not path.is_dir():
        raise ValueError(f"bundle must be a real directory: {path}")
    entries = []
    for item in sorted(path.rglob("*")):
        if item.is_symlink():
            raise ValueError(f"bundle contains symlink: {item}")
        if item.is_file():
            entries.append((item.relative_to(path).as_posix(), hashlib.sha256(item.read_bytes()).hexdigest()))
    return hashlib.sha256(json.dumps(entries, separators=(",", ":")).encode()).hexdigest()


def durable_write(path, data):
    path.parent.mkdir(parents=True, exist_ok=True)
    temp = path.with_name(path.name + ".tmp-" + uuid.uuid4().hex)
    try:
        with temp.open("x") as stream:
            json.dump(data, stream, sort_keys=True, indent=2)
            stream.write("\n")
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(temp, path)
        fd = os.open(path.parent, os.O_RDONLY)
        try:
            os.fsync(fd)
        finally:
            os.close(fd)
    finally:
        temp.unlink(missing_ok=True)


def set_phase(journal_path, journal, phase, fault=None):
    journal["phase"] = phase
    journal["updated_at_monotonic_ns"] = time.monotonic_ns()
    durable_write(journal_path, journal)
    if fault == phase:
        raise RuntimeError(f"injected failure after journal phase {phase}")


def inject_after(fault, checkpoint):
    if fault == checkpoint:
        raise RuntimeError(f"injected recovery interruption after {checkpoint}")


def checked_paths(candidate, active, state):
    candidate = Path(candidate).absolute()
    active = Path(active).absolute()
    state = Path(state).absolute()
    if candidate == active or active == active.parent or state == state.parent:
        raise ValueError("unsafe transaction path")
    if candidate.is_symlink() or active.is_symlink() or state.is_symlink():
        raise ValueError("candidate, active, and transaction state paths cannot be symlinks")
    candidate = candidate.resolve(strict=False)
    active = active.resolve(strict=False)
    state = state.resolve(strict=False)
    if candidate == active or candidate in active.parents or active in candidate.parents:
        raise ValueError("candidate and active bundle paths must not contain one another")
    if state == active or state in active.parents or active in state.parents:
        raise ValueError("transaction state must be outside the active bundle")
    return candidate, active, state


def install(candidate, active, state, fault=None):
    candidate, active, state = checked_paths(candidate, active, state)
    candidate_hash = tree_hash(candidate)
    if candidate_hash is None:
        raise ValueError("candidate bundle is missing")
    state.mkdir(parents=True, exist_ok=True)
    journal_path = state / "transaction.json"
    if journal_path.exists():
        previous = json.loads(journal_path.read_text())
        if previous.get("schema") != SCHEMA or previous.get("phase") not in ("committed", "rollback_validated"):
            raise RuntimeError("unreconciled transaction journal exists; run recover first")
        history = state / "history"
        history.mkdir(parents=True, exist_ok=True)
        archived = history / (previous.get("operation_id", "unknown") + ".json")
        if archived.exists():
            raise RuntimeError("transaction history identity already exists")
        os.replace(journal_path, archived)
    opid = uuid.uuid4().hex
    stage = active.parent / ("." + active.name + ".stage-" + opid)
    backup = state / ("baseline-" + opid)
    original_hash = tree_hash(active)
    journal = {
        "schema": SCHEMA, "operation_id": opid, "phase": "created",
        "candidate": str(candidate), "candidate_sha256": candidate_hash,
        "active": str(active), "original_sha256": original_hash,
        "original_present": original_hash is not None, "stage": str(stage),
        "backup": str(backup), "created_at_monotonic_ns": time.monotonic_ns(),
    }
    durable_write(journal_path, journal)
    try:
        set_phase(journal_path, journal, "preflight", fault)
        shutil.copytree(candidate, stage)
        if tree_hash(stage) != candidate_hash:
            raise RuntimeError("staged candidate hash mismatch")
        set_phase(journal_path, journal, "staged", fault)
        if original_hash is not None:
            os.replace(active, backup)
        set_phase(journal_path, journal, "backup_moved", fault)
        os.replace(stage, active)
        set_phase(journal_path, journal, "replaced", fault)
        if tree_hash(active) != candidate_hash:
            raise RuntimeError("installed candidate hash mismatch")
        set_phase(journal_path, journal, "validated", fault)
        journal["result"] = "PASS"
        set_phase(journal_path, journal, "committed", fault)
        return journal
    except BaseException:
        recover(state)
        raise


def recover(state, fault=None):
    state = Path(state).absolute()
    journal_path = state / "transaction.json"
    if not journal_path.exists():
        return {"result": "NO_PENDING_TRANSACTION"}
    journal = json.loads(journal_path.read_text())
    if journal.get("schema") != SCHEMA:
        raise RuntimeError("unsupported or corrupt transaction journal; refusing mutation")
    if journal.get("phase") == "committed":
        return {"result": "ALREADY_COMMITTED", "operation_id": journal["operation_id"]}
    if journal.get("phase") == "rollback_validated":
        return {"result": "ALREADY_ROLLED_BACK", "operation_id": journal["operation_id"]}
    candidate, active, state_check = checked_paths(journal["candidate"], journal["active"], state)
    if state_check != state:
        raise RuntimeError("journal state path mismatch")
    stage, backup = Path(journal["stage"]), Path(journal["backup"])
    if stage.parent != active.parent or backup.parent != state:
        raise RuntimeError("journal contains paths outside transaction roots")
    candidate_hash = journal["candidate_sha256"]
    original_hash = journal["original_sha256"]
    set_phase(journal_path, journal, "rollback", fault)
    if active.exists() and tree_hash(active) == candidate_hash:
        rejected = state / ("rejected-" + journal["operation_id"])
        if rejected.exists():
            raise RuntimeError("rejected bundle path already exists")
        os.replace(active, rejected)
        inject_after(fault, "rollback_rejected")
    elif active.exists() and original_hash is None:
        raise RuntimeError("unexpected active bundle during rollback; preserving it")
    if backup.exists():
        if tree_hash(backup) != original_hash:
            raise RuntimeError("baseline backup hash mismatch; preserving all transaction data")
        if active.exists():
            raise RuntimeError("active path occupied before baseline restore")
        os.replace(backup, active)
        inject_after(fault, "rollback_restored")
    elif original_hash is not None and (not active.exists() or tree_hash(active) != original_hash):
        raise RuntimeError("baseline backup missing or active identity differs; manual recovery required")
    if stage.exists():
        shutil.rmtree(stage)
        inject_after(fault, "rollback_stage_removed")
    if original_hash is None and active.exists():
        raise RuntimeError("original absence was not restored")
    if original_hash is not None and tree_hash(active) != original_hash:
        raise RuntimeError("rollback validation failed")
    journal["result"] = "ROLLED_BACK"
    set_phase(journal_path, journal, "rollback_validated", fault)
    return {"result": "ROLLED_BACK", "operation_id": journal["operation_id"],
            "restored_sha256": original_hash}


def main(argv=None):
    parser = argparse.ArgumentParser()
    sub = parser.add_subparsers(dest="action", required=True)
    install_cmd = sub.add_parser("install")
    install_cmd.add_argument("--candidate", required=True)
    install_cmd.add_argument("--active", required=True)
    install_cmd.add_argument("--state", required=True)
    install_cmd.add_argument("--test-fail-after", choices=PHASES)
    recover_cmd = sub.add_parser("recover")
    recover_cmd.add_argument("--state", required=True)
    status_cmd = sub.add_parser("status")
    status_cmd.add_argument("--state", required=True)
    args = parser.parse_args(argv)
    try:
        if args.action == "install":
            result = install(args.candidate, args.active, args.state, args.test_fail_after)
        elif args.action == "recover":
            result = recover(args.state)
        else:
            journal = Path(args.state) / "transaction.json"
            result = json.loads(journal.read_text()) if journal.exists() else {"result": "NO_PENDING_TRANSACTION"}
        print(json.dumps(result, sort_keys=True))
        return 0
    except Exception as exc:
        print(json.dumps({"result": "FAIL", "error": str(exc)}, sort_keys=True), file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
