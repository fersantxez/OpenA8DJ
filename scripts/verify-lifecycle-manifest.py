#!/usr/bin/env python3
"""Verify that recorded build inputs and optional candidate bytes still match."""
import argparse
import hashlib
import json
from pathlib import Path
import sys


def digest(path):
    h = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()


def validate(manifest, root, require_bundle=False):
    errors = []
    if manifest.get("schema") != "opena8dj.macos-lifecycle-run-manifest.v1":
        errors.append("unsupported_schema")
    inputs = manifest.get("repository", {}).get("source_inputs", {})
    for rel, expected in inputs.items():
        relpath = Path(rel)
        if relpath.is_absolute() or ".." in relpath.parts:
            errors.append(f"unsafe_source_path:{rel}")
            continue
        path = root / relpath
        if not path.is_file():
            errors.append(f"missing_source:{rel}")
        elif digest(path) != expected:
            errors.append(f"source_hash_mismatch:{rel}")
    expected_tree = hashlib.sha256(json.dumps(inputs, sort_keys=True).encode()).hexdigest()
    if manifest.get("repository", {}).get("source_inputs_sha256") != expected_tree:
        errors.append("source_input_manifest_mismatch")
    build = manifest.get("build", {})
    if require_bundle and not build.get("bundle_sha256"):
        errors.append("candidate_bundle_hash_missing")
    if require_bundle:
        signature = build.get("bundle_signature_verification", {})
        if signature.get("exit_code") != 0:
            errors.append("candidate_bundle_signature_unverified")
        if not build.get("binary_uuid", {}).get("stdout"):
            errors.append("candidate_binary_uuid_missing")
    if build.get("bundle_sha256"):
        bundle_rel = Path(build.get("bundle_path", ""))
        path = bundle_rel if bundle_rel.is_absolute() else root / bundle_rel
        if not path.is_file() and not path.is_dir():
            errors.append("candidate_bundle_missing")
        else:
            actual = digest(path) if path.is_file() else digest_tree(path)
            if actual != build["bundle_sha256"]:
                errors.append("candidate_bundle_hash_mismatch")
    return errors


def digest_tree(path):
    items = []
    for child in sorted(p for p in path.rglob("*") if p.is_file()):
        items.append((str(child.relative_to(path)), digest(child)))
    return hashlib.sha256(json.dumps(items, separators=(",", ":")).encode()).hexdigest()


def main(argv=None):
    parser = argparse.ArgumentParser()
    parser.add_argument("manifest", type=Path)
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--require-bundle", action="store_true")
    args = parser.parse_args(argv)
    try:
        manifest = json.loads(args.manifest.read_text())
    except (OSError, json.JSONDecodeError) as exc:
        print(f"lifecycle_manifest=INFRA_ERROR\nerror={exc}")
        return 2
    errors = validate(manifest, args.root.resolve(), args.require_bundle)
    print(f"lifecycle_manifest={'PASS' if not errors else 'FAIL'}")
    for error in errors:
        print(f"error={error}")
    return 0 if not errors else 1


if __name__ == "__main__":
    raise SystemExit(main())
