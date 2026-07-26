#!/usr/bin/env python3
"""Build experimental OpenA8DJ Linux installer packages.

The generated packages are intentionally diagnostic-only. They install
user-space tools, documentation, profile metadata, and a conservative udev rule.
They do not install or load a kernel module.
"""

from __future__ import annotations

import argparse
import gzip
import hashlib
import io
import json
import os
import shutil
import stat
import struct
import subprocess
import tarfile
import tempfile
import time
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Iterable


PACKAGE_NAME = "opena8dj-linux-experimental"
TOOL_VERSION = "0.1.1"
DEB_VERSION = "0.1.1~experimental20260726"
RPM_VERSION = "0.1.1"
RPM_RELEASE = "0.experimental20260726"
DIST_ID = f"{PACKAGE_NAME}-{DEB_VERSION}"
READINESS_LABEL = "diagnostic only, sound quality not validated"
DRIVER_CHANNEL = "in-kernel snd-usb-caiaq"
BUILD_MTIME = 1785024000  # 2026-07-26T00:00:00Z


RPM_NULL = 0
RPM_CHAR = 1
RPM_INT8 = 2
RPM_INT16 = 3
RPM_INT32 = 4
RPM_INT64 = 5
RPM_STRING = 6
RPM_BIN = 7
RPM_STRING_ARRAY = 8
RPM_I18NSTRING = 9


@dataclass(frozen=True)
class InstallEntry:
    source: Path
    dest: str
    mode: int


@dataclass(frozen=True)
class PayloadEntry:
    path: str
    data: bytes
    mode: int
    is_dir: bool = False


def repo_root() -> Path:
    return Path(__file__).resolve().parents[2]


def run(args: list[str], cwd: Path | None = None) -> str:
    completed = subprocess.run(
        args,
        cwd=str(cwd) if cwd else None,
        check=False,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )
    if completed.returncode != 0:
        raise RuntimeError(completed.stderr.strip() or completed.stdout.strip())
    return completed.stdout.strip()


def git_value(args: list[str], root: Path, fallback: str) -> str:
    try:
        return run(["git", *args], cwd=root)
    except Exception:
        return fallback


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def read_bytes(path: Path) -> bytes:
    return path.read_bytes()


def package_entries(root: Path, candidate_json: bytes) -> list[PayloadEntry]:
    files: list[InstallEntry] = [
        InstallEntry(
            root / "linux/tools/opena8dj-linuxctl",
            "/usr/bin/opena8dj-linuxctl",
            0o755,
        ),
        InstallEntry(
            root / "linux/packaging/common/70-opena8dj-audio8dj.rules",
            "/usr/lib/udev/rules.d/70-opena8dj-audio8dj.rules",
            0o644,
        ),
        InstallEntry(
            root / "linux/packaging/common/profile-schema.json",
            f"/usr/share/{PACKAGE_NAME}/profile-schema.json",
            0o644,
        ),
        InstallEntry(
            root / "linux/packaging/common/README-FIRST.md",
            f"/usr/share/doc/{PACKAGE_NAME}/README-FIRST.md",
            0o644,
        ),
        InstallEntry(
            root / "linux/IMPLEMENTATION_PLAN.md",
            f"/usr/share/doc/{PACKAGE_NAME}/IMPLEMENTATION_PLAN.md",
            0o644,
        ),
        InstallEntry(
            root / "linux/ENUMERATION_PLAN.md",
            f"/usr/share/doc/{PACKAGE_NAME}/ENUMERATION_PLAN.md",
            0o644,
        ),
        InstallEntry(
            root / "linux/QUALITY_AND_PERFORMANCE_GATES.md",
            f"/usr/share/doc/{PACKAGE_NAME}/QUALITY_AND_PERFORMANCE_GATES.md",
            0o644,
        ),
        InstallEntry(
            root / "linux/CANDIDATE_PAYLOAD.md",
            f"/usr/share/doc/{PACKAGE_NAME}/CANDIDATE_PAYLOAD.md",
            0o644,
        ),
        InstallEntry(
            root / "linux/MACOS_WINDOWS_HARDWARE_LESSONS.md",
            f"/usr/share/doc/{PACKAGE_NAME}/MACOS_WINDOWS_HARDWARE_LESSONS.md",
            0o644,
        ),
        InstallEntry(
            root / "linux/LEGAL_AND_PROVENANCE.md",
            f"/usr/share/doc/{PACKAGE_NAME}/LEGAL_AND_PROVENANCE.md",
            0o644,
        ),
        InstallEntry(
            root / "linux/SND_USB_CAIAQ_AUDIT.md",
            f"/usr/share/doc/{PACKAGE_NAME}/SND_USB_CAIAQ_AUDIT.md",
            0o644,
        ),
        InstallEntry(
            root / "LICENSE",
            f"/usr/share/doc/{PACKAGE_NAME}/LICENSE",
            0o644,
        ),
        InstallEntry(
            root / "NOTICE.md",
            f"/usr/share/doc/{PACKAGE_NAME}/NOTICE.md",
            0o644,
        ),
    ]

    entries: list[PayloadEntry] = []
    dirs = {
        "/usr",
        "/usr/bin",
        "/usr/lib",
        "/usr/lib/udev",
        "/usr/lib/udev/rules.d",
        "/usr/share",
        f"/usr/share/{PACKAGE_NAME}",
        "/usr/share/doc",
        f"/usr/share/doc/{PACKAGE_NAME}",
    }
    for directory in sorted(dirs):
        entries.append(PayloadEntry(directory, b"", 0o40755, is_dir=True))

    for item in files:
        entries.append(PayloadEntry(item.dest, read_bytes(item.source), 0o100000 | item.mode))

    entries.append(
        PayloadEntry(
            f"/usr/share/doc/{PACKAGE_NAME}/opena8dj-linux-candidate.json",
            candidate_json,
            0o100644,
        )
    )
    return entries


def tar_add_payload(tar: tarfile.TarFile, entry: PayloadEntry, root_prefix: str = ".") -> None:
    name = f"{root_prefix}{entry.path}"
    info = tarfile.TarInfo(name)
    info.mtime = BUILD_MTIME
    info.uid = 0
    info.gid = 0
    info.uname = "root"
    info.gname = "root"
    info.mode = entry.mode & 0o7777
    if entry.is_dir:
        info.type = tarfile.DIRTYPE
        info.size = 0
        tar.addfile(info)
    else:
        info.size = len(entry.data)
        tar.addfile(info, io.BytesIO(entry.data))


def make_tar_gz(entries: list[PayloadEntry], output: Path) -> None:
    with gzip.GzipFile(filename="", mode="wb", fileobj=output.open("wb"), mtime=BUILD_MTIME) as gz:
        with tarfile.open(fileobj=gz, mode="w") as tar:
            for entry in entries:
                tar_add_payload(tar, entry, root_prefix=PACKAGE_NAME)


def make_data_tar(entries: list[PayloadEntry], output: Path) -> None:
    with gzip.GzipFile(filename="", mode="wb", fileobj=output.open("wb"), mtime=BUILD_MTIME) as gz:
        with tarfile.open(fileobj=gz, mode="w") as tar:
            for entry in entries:
                tar_add_payload(tar, entry, root_prefix=".")


def make_control_tar(control_files: dict[str, tuple[bytes, int]], output: Path) -> None:
    with gzip.GzipFile(filename="", mode="wb", fileobj=output.open("wb"), mtime=BUILD_MTIME) as gz:
        with tarfile.open(fileobj=gz, mode="w") as tar:
            for name, (data, mode) in control_files.items():
                info = tarfile.TarInfo(f"./{name}")
                info.mtime = BUILD_MTIME
                info.uid = 0
                info.gid = 0
                info.uname = "root"
                info.gname = "root"
                info.mode = mode
                info.size = len(data)
                tar.addfile(info, io.BytesIO(data))


def deb_control(installed_size_kb: int) -> bytes:
    text = f"""Package: {PACKAGE_NAME}
Version: {DEB_VERSION}
Section: sound
Priority: optional
Architecture: all
Maintainer: OpenA8DJ Experimental <noreply@opena8dj.local>
Depends: python3, alsa-utils, usbutils
Installed-Size: {installed_size_kb}
Homepage: https://github.com/fersantxez/OpenA8DJ
Description: Experimental OpenA8DJ Linux tools for Audio 8 DJ
 This package installs diagnostic-only OpenA8DJ Linux tooling, documentation,
 profile metadata, and a conservative udev tag for Native Instruments Audio 8
 DJ USB id 17cc:1978. It relies on the in-kernel snd-usb-caiaq driver and does
 not install, load, bind, unbind, or validate a replacement kernel module.
 .
 Readiness: {READINESS_LABEL}.
"""
    return text.encode("utf-8")


def make_deb(entries: list[PayloadEntry], output: Path) -> None:
    installed_size = sum(len(entry.data) for entry in entries if not entry.is_dir)
    installed_size_kb = max(1, (installed_size + 1023) // 1024)
    postinst = f"""#!/bin/sh
set -e
echo "{PACKAGE_NAME} installed."
echo "{READINESS_LABEL}"
echo "No module was loaded, no USB device was reset, and no audio test was run."
exit 0
""".encode("utf-8")
    prerm = b"""#!/bin/sh
set -e
exit 0
"""

    with tempfile.TemporaryDirectory() as tmp_name:
        tmp = Path(tmp_name)
        debian_binary = tmp / "debian-binary"
        control_tar = tmp / "control.tar.gz"
        data_tar = tmp / "data.tar.gz"
        debian_binary.write_text("2.0\n", encoding="ascii")
        make_control_tar(
            {
                "control": (deb_control(installed_size_kb), 0o644),
                "postinst": (postinst, 0o755),
                "prerm": (prerm, 0o755),
            },
            control_tar,
        )
        make_data_tar(entries, data_tar)
        if output.exists():
            output.unlink()
        run(["ar", "-qS", str(output), "debian-binary", "control.tar.gz", "data.tar.gz"], cwd=tmp)


def align_bytes(data: bytes, boundary: int) -> bytes:
    padding = (-len(data)) % boundary
    if padding:
        return data + (b"\x00" * padding)
    return data


def cpio_hex(value: int) -> bytes:
    return f"{value & 0xFFFFFFFF:08x}".encode("ascii")


def make_cpio(entries: list[PayloadEntry]) -> bytes:
    output = io.BytesIO()
    inode = 1

    def add(name: str, mode: int, data: bytes, is_dir: bool) -> None:
        nonlocal inode
        encoded_name = name.encode("utf-8") + b"\x00"
        fields = [
            b"070701",
            cpio_hex(inode),
            cpio_hex(mode),
            cpio_hex(0),
            cpio_hex(0),
            cpio_hex(2 if is_dir else 1),
            cpio_hex(BUILD_MTIME),
            cpio_hex(len(data)),
            cpio_hex(0),
            cpio_hex(0),
            cpio_hex(0),
            cpio_hex(0),
            cpio_hex(len(encoded_name)),
            cpio_hex(0),
        ]
        output.write(b"".join(fields))
        output.write(encoded_name)
        output.write(b"\x00" * ((4 - (output.tell() % 4)) % 4))
        output.write(data)
        output.write(b"\x00" * ((4 - (output.tell() % 4)) % 4))
        inode += 1

    for entry in entries:
        cpio_name = "." + entry.path
        add(cpio_name, entry.mode, entry.data, entry.is_dir)

    add("TRAILER!!!", 0, b"", False)
    return output.getvalue()


def rpm_encode_value(kind: int, value: Any) -> tuple[bytes, int]:
    if kind == RPM_STRING:
        return str(value).encode("utf-8") + b"\x00", 1
    if kind in (RPM_STRING_ARRAY, RPM_I18NSTRING):
        values = list(value)
        return b"".join(str(item).encode("utf-8") + b"\x00" for item in values), len(values)
    if kind == RPM_INT16:
        values = list(value)
        return b"".join(struct.pack(">H", int(item) & 0xFFFF) for item in values), len(values)
    if kind == RPM_INT32:
        values = list(value)
        return b"".join(struct.pack(">I", int(item) & 0xFFFFFFFF) for item in values), len(values)
    if kind == RPM_BIN:
        raw = bytes(value)
        return raw, len(raw)
    raise ValueError(f"unsupported RPM type {kind}")


def rpm_type_alignment(kind: int) -> int:
    if kind == RPM_INT16:
        return 2
    if kind == RPM_INT32:
        return 4
    if kind == RPM_INT64:
        return 8
    return 1


def make_rpm_header(tags: list[tuple[int, int, Any]]) -> bytes:
    store = b""
    indexes = []
    for tag, kind, value in tags:
        store = align_bytes(store, rpm_type_alignment(kind))
        offset = len(store)
        encoded, count = rpm_encode_value(kind, value)
        store += encoded
        indexes.append(struct.pack(">IIII", tag, kind, offset, count))
    header = b"\x8e\xad\xe8\x01" + struct.pack(">III", 0, len(indexes), len(store))
    return header + b"".join(indexes) + store


def rpm_file_metadata(entries: list[PayloadEntry]) -> dict[str, list[Any]]:
    basenames: list[str] = []
    dirnames: list[str] = []
    dirindexes: list[int] = []
    sizes: list[int] = []
    modes: list[int] = []
    rdevs: list[int] = []
    mtimes: list[int] = []
    digests: list[str] = []
    linktos: list[str] = []
    flags: list[int] = []
    users: list[str] = []
    groups: list[str] = []
    verifyflags: list[int] = []
    devices: list[int] = []
    inodes: list[int] = []
    langs: list[str] = []
    colors: list[int] = []

    dirname_to_index: dict[str, int] = {}
    for inode, entry in enumerate(entries, start=1):
        path = entry.path
        parent = str(Path(path).parent)
        if parent == "/":
            dirname = "/"
        else:
            dirname = parent + "/"
        basename = Path(path).name
        if not basename:
            basename = path.strip("/")
            dirname = "/"
        if dirname not in dirname_to_index:
            dirname_to_index[dirname] = len(dirnames)
            dirnames.append(dirname)
        basenames.append(basename)
        dirindexes.append(dirname_to_index[dirname])
        sizes.append(0 if entry.is_dir else len(entry.data))
        modes.append(entry.mode)
        rdevs.append(0)
        mtimes.append(BUILD_MTIME)
        digests.append("" if entry.is_dir else hashlib.md5(entry.data).hexdigest())
        linktos.append("")
        flags.append(0)
        users.append("root")
        groups.append("root")
        verifyflags.append(0)
        devices.append(1)
        inodes.append(inode)
        langs.append("")
        colors.append(0)

    return {
        "basenames": basenames,
        "dirnames": dirnames,
        "dirindexes": dirindexes,
        "sizes": sizes,
        "modes": modes,
        "rdevs": rdevs,
        "mtimes": mtimes,
        "digests": digests,
        "linktos": linktos,
        "flags": flags,
        "users": users,
        "groups": groups,
        "verifyflags": verifyflags,
        "devices": devices,
        "inodes": inodes,
        "langs": langs,
        "colors": colors,
    }


def make_rpm(entries: list[PayloadEntry], output: Path, candidate_id: str) -> None:
    cpio = make_cpio(entries)
    payload = gzip.compress(cpio, compresslevel=9, mtime=BUILD_MTIME)
    meta = rpm_file_metadata(entries)
    payload_size = len(payload)
    install_size = sum(len(entry.data) for entry in entries if not entry.is_dir)

    main_tags = [
        (1000, RPM_STRING, PACKAGE_NAME),
        (1001, RPM_STRING, RPM_VERSION),
        (1002, RPM_STRING, RPM_RELEASE),
        (1004, RPM_I18NSTRING, ["Experimental OpenA8DJ Linux tools for Audio 8 DJ"]),
        (
            1005,
            RPM_I18NSTRING,
            [
                "Diagnostic-only OpenA8DJ Linux tooling, documentation, profile "
                "metadata, and udev tagging. Relies on in-kernel snd-usb-caiaq."
            ],
        ),
        (1006, RPM_INT32, [BUILD_MTIME]),
        (1007, RPM_STRING, "opena8dj-linux-agent"),
        (1009, RPM_INT32, [install_size]),
        (1014, RPM_STRING, "MIT and documentation; kernel driver is in-kernel GPL snd-usb-caiaq"),
        (1016, RPM_STRING, "Applications/Multimedia"),
        (1021, RPM_STRING, "linux"),
        (1022, RPM_STRING, "noarch"),
        (1028, RPM_INT32, meta["sizes"]),
        (1030, RPM_INT16, meta["modes"]),
        (1033, RPM_INT16, meta["rdevs"]),
        (1034, RPM_INT32, meta["mtimes"]),
        (1035, RPM_STRING_ARRAY, meta["digests"]),
        (1036, RPM_STRING_ARRAY, meta["linktos"]),
        (1037, RPM_INT32, meta["flags"]),
        (1039, RPM_STRING_ARRAY, meta["users"]),
        (1040, RPM_STRING_ARRAY, meta["groups"]),
        (1044, RPM_STRING, f"{PACKAGE_NAME}-{RPM_VERSION}-{RPM_RELEASE}.src.rpm"),
        (1045, RPM_INT32, meta["verifyflags"]),
        (1046, RPM_INT32, [payload_size]),
        (1095, RPM_INT32, meta["devices"]),
        (1096, RPM_INT32, meta["inodes"]),
        (1097, RPM_STRING_ARRAY, meta["langs"]),
        (1112, RPM_STRING, candidate_id),
        (1116, RPM_INT32, meta["dirindexes"]),
        (1117, RPM_STRING_ARRAY, meta["basenames"]),
        (1118, RPM_STRING_ARRAY, meta["dirnames"]),
        (1122, RPM_STRING, ""),
        (1124, RPM_STRING, "cpio"),
        (1125, RPM_STRING, "gzip"),
        (1126, RPM_STRING, "9"),
        (1131, RPM_STRING, "noarch-unknown-linux"),
        (1132, RPM_STRING_ARRAY, ["noarch-unknown-linux"]),
        (1140, RPM_INT32, meta["colors"]),
        (1155, RPM_STRING_ARRAY, [PACKAGE_NAME]),
        (1156, RPM_STRING_ARRAY, ["="]),
        (1157, RPM_STRING_ARRAY, [f"{RPM_VERSION}-{RPM_RELEASE}"]),
        (5011, RPM_STRING, READINESS_LABEL),
    ]
    main_header = align_bytes(make_rpm_header(main_tags), 8)

    sig_tags = [
        (1000, RPM_INT32, [len(main_header) + len(payload)]),
        (1007, RPM_INT32, [len(payload)]),
    ]
    sig_header = align_bytes(make_rpm_header(sig_tags), 8)

    name = f"{PACKAGE_NAME}-{RPM_VERSION}-{RPM_RELEASE}".encode("ascii")[:65]
    lead_name = name + (b"\x00" * (66 - len(name)))
    lead = struct.pack(
        ">4sBBHH66sHH16s",
        b"\xed\xab\xee\xdb",
        3,
        0,
        0,
        255,
        lead_name,
        1,
        5,
        b"\x00" * 16,
    )
    output.write_bytes(lead + sig_header + main_header + payload)


def base_candidate_metadata(root: Path) -> dict[str, Any]:
    branch = git_value(["branch", "--show-current"], root, "unknown")
    commit = git_value(["rev-parse", "HEAD"], root, "unknown")
    dirty = bool(git_value(["status", "--short"], root, ""))
    return {
        "name": "opena8dj-linux",
        "package_name": PACKAGE_NAME,
        "candidate_id": DIST_ID,
        "git_commit": commit,
        "git_dirty": dirty,
        "branch": branch,
        "driver_channel": DRIVER_CHANNEL,
        "module_name": "snd-usb-caiaq",
        "hardware_model_schema": "org.opena8dj.linux.hardware-model.v1",
        "cross_platform_lessons": {
            "macos_reference": [
                "OpenA8DJUSB.m USB transport and stream counters",
                "opena8dj-control profile/control surface",
                "physical music quality gate and external capture discipline",
            ],
            "windows_reference": [
                "OpenA8DJShared.h API v2 surface",
                "OpenA8DJAudioEngine offline contract",
                "Windows ACX/KMDF design locks",
            ],
        },
        "kernel_targets": [
            "distribution kernel with CONFIG_SND_USB_CAIAQ enabled"
        ],
        "packages": [],
        "hashes": {},
        "validation_label": READINESS_LABEL,
        "hardware_tests_run": False,
        "playback_capture_tested": False,
        "physical_sound_quality_validated": False,
        "secure_boot_tested": False,
        "native_instruments_payload_included": False,
    }


def write_json(path: Path, data: dict[str, Any]) -> None:
    path.write_text(json.dumps(data, indent=2, sort_keys=True) + "\n", encoding="utf-8")


def copy_readme(root: Path, output_dir: Path) -> None:
    shutil.copy2(root / "linux/packaging/common/README-FIRST.md", output_dir / "README-FIRST.md")


def write_checksums(output_dir: Path, artifacts: Iterable[Path]) -> None:
    lines = []
    for artifact in sorted(artifacts, key=lambda p: p.name):
        lines.append(f"{sha256_file(artifact)}  {artifact.name}")
    (output_dir / "SHA256SUMS").write_text("\n".join(lines) + "\n", encoding="utf-8")


def build(output_base: Path) -> dict[str, Any]:
    root = repo_root()
    output_dir = output_base / DIST_ID
    if output_dir.exists():
        shutil.rmtree(output_dir)
    output_dir.mkdir(parents=True)

    candidate = base_candidate_metadata(root)
    candidate_json_initial = json.dumps(candidate, indent=2, sort_keys=True).encode("utf-8") + b"\n"
    entries = package_entries(root, candidate_json_initial)

    deb = output_dir / f"{PACKAGE_NAME}_{DEB_VERSION}_all.deb"
    rpm = output_dir / f"{PACKAGE_NAME}-{RPM_VERSION}-{RPM_RELEASE}.noarch.rpm"
    tarball = output_dir / f"{PACKAGE_NAME}-{DEB_VERSION}.tar.gz"

    make_deb(entries, deb)
    make_rpm(entries, rpm, DIST_ID)
    make_tar_gz(entries, tarball)

    artifacts = [deb, rpm, tarball]
    candidate["packages"] = [
        {"path": artifact.name, "sha256": sha256_file(artifact)}
        for artifact in artifacts
    ]
    candidate["hashes"] = {artifact.name: sha256_file(artifact) for artifact in artifacts}
    candidate["built_at_unix"] = int(time.time())
    candidate["readme"] = "README-FIRST.md"

    candidate_path = output_dir / "opena8dj-linux-candidate.json"
    write_json(candidate_path, candidate)
    copy_readme(root, output_dir)

    all_artifacts = [*artifacts, candidate_path, output_dir / "README-FIRST.md"]
    write_checksums(output_dir, all_artifacts)
    return {"output_dir": str(output_dir), "artifacts": [str(path) for path in all_artifacts]}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--output-base",
        default=str(repo_root() / "dist/linux/experimental"),
        help="directory under which the package set will be written",
    )
    args = parser.parse_args()
    result = build(Path(args.output_base))
    print(json.dumps(result, indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
