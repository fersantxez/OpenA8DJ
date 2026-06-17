#!/usr/bin/env python3
"""Verify the stream-stats IPC payload contract shared by HAL and control."""

from __future__ import annotations

import json
import re
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
HAL_PATH = ROOT / "src/hal/OpenA8DJUSB.m"
CONTROL_PATH = ROOT / "src/tools/opena8dj-control.c"
STRUCT_NAME = "OpenA8DJStreamStatsPayload"


def extract_fields(path: Path) -> list[dict[str, str]]:
    text = path.read_text(encoding="utf-8", errors="replace")
    pattern = re.compile(
        rf"typedef\s+struct\s+{STRUCT_NAME}\s*\{{(?P<body>.*?)\}}\s*"
        rf"__attribute__\(\(packed\)\)\s*{STRUCT_NAME}\s*;",
        re.DOTALL,
    )
    match = pattern.search(text)
    if match is None:
        raise ValueError(f"{STRUCT_NAME} not found in {path}")

    fields: list[dict[str, str]] = []
    for raw_line in match.group("body").splitlines():
        line = raw_line.split("//", 1)[0].strip()
        if not line:
            continue
        field_match = re.match(r"(?P<type>[A-Za-z0-9_]+)\s+(?P<name>[A-Za-z0-9_]+)\s*;", line)
        if field_match is None:
            raise ValueError(f"unsupported field syntax in {path}: {raw_line!r}")
        fields.append(field_match.groupdict())
    return fields


def main() -> int:
    try:
        hal_fields = extract_fields(HAL_PATH)
        control_fields = extract_fields(CONTROL_PATH)
    except Exception as exc:  # noqa: BLE001 - this is a CLI contract report.
        print(
            json.dumps(
                {
                    "schema": "opena8djcpp.stream-stats-contract.v1",
                    "result": "FAIL",
                    "error": str(exc),
                },
                indent=2,
                sort_keys=True,
            )
        )
        return 1

    mismatches = []
    max_len = max(len(hal_fields), len(control_fields))
    for index in range(max_len):
        hal = hal_fields[index] if index < len(hal_fields) else None
        control = control_fields[index] if index < len(control_fields) else None
        if hal != control:
            mismatches.append(
                {
                    "index": index,
                    "hal": hal,
                    "control": control,
                }
            )

    result = {
        "schema": "opena8djcpp.stream-stats-contract.v1",
        "result": "PASS" if not mismatches else "FAIL",
        "struct": STRUCT_NAME,
        "hal_path": str(HAL_PATH.relative_to(ROOT)),
        "control_path": str(CONTROL_PATH.relative_to(ROOT)),
        "field_count": len(hal_fields),
        "control_field_count": len(control_fields),
        "mismatch_count": len(mismatches),
        "mismatches": mismatches[:16],
        "last_field": hal_fields[-1]["name"] if hal_fields else None,
    }
    print(json.dumps(result, indent=2, sort_keys=True))
    return 0 if not mismatches else 1


if __name__ == "__main__":
    sys.exit(main())
