#!/usr/bin/env python3
import argparse
import ctypes
import json
from ctypes import wintypes
from pathlib import Path


MAXPNAMELEN = 32
MMSYSERR_NOERROR = 0


class MIDIINCAPSW(ctypes.Structure):
    _fields_ = [
        ("wMid", wintypes.WORD),
        ("wPid", wintypes.WORD),
        ("vDriverVersion", wintypes.UINT),
        ("szPname", wintypes.WCHAR * MAXPNAMELEN),
        ("dwSupport", wintypes.DWORD),
    ]


class MIDIOUTCAPSW(ctypes.Structure):
    _fields_ = [
        ("wMid", wintypes.WORD),
        ("wPid", wintypes.WORD),
        ("vDriverVersion", wintypes.UINT),
        ("szPname", wintypes.WCHAR * MAXPNAMELEN),
        ("wTechnology", wintypes.WORD),
        ("wVoices", wintypes.WORD),
        ("wNotes", wintypes.WORD),
        ("wChannelMask", wintypes.WORD),
        ("dwSupport", wintypes.DWORD),
    ]


def is_a8dj_name(name):
    lowered = name.lower()
    needles = ("opena8dj", "open a8dj", "audio 8 dj", "a8dj", "native instruments")
    return any(needle in lowered for needle in needles)


def enumerate_midi():
    winmm = ctypes.WinDLL("winmm")
    winmm.midiInGetNumDevs.restype = wintypes.UINT
    winmm.midiOutGetNumDevs.restype = wintypes.UINT
    winmm.midiInGetDevCapsW.argtypes = [wintypes.UINT, ctypes.POINTER(MIDIINCAPSW), wintypes.UINT]
    winmm.midiOutGetDevCapsW.argtypes = [wintypes.UINT, ctypes.POINTER(MIDIOUTCAPSW), wintypes.UINT]

    inputs = []
    outputs = []
    for index in range(int(winmm.midiInGetNumDevs())):
        caps = MIDIINCAPSW()
        status = int(winmm.midiInGetDevCapsW(index, ctypes.byref(caps), ctypes.sizeof(caps)))
        inputs.append(
            {
                "index": index,
                "status": status,
                "ok": status == MMSYSERR_NOERROR,
                "name": caps.szPname if status == MMSYSERR_NOERROR else "",
                "matches_audio8dj": status == MMSYSERR_NOERROR and is_a8dj_name(caps.szPname),
                "manufacturer_id": int(caps.wMid) if status == MMSYSERR_NOERROR else None,
                "product_id": int(caps.wPid) if status == MMSYSERR_NOERROR else None,
            }
        )
    for index in range(int(winmm.midiOutGetNumDevs())):
        caps = MIDIOUTCAPSW()
        status = int(winmm.midiOutGetDevCapsW(index, ctypes.byref(caps), ctypes.sizeof(caps)))
        outputs.append(
            {
                "index": index,
                "status": status,
                "ok": status == MMSYSERR_NOERROR,
                "name": caps.szPname if status == MMSYSERR_NOERROR else "",
                "matches_audio8dj": status == MMSYSERR_NOERROR and is_a8dj_name(caps.szPname),
                "manufacturer_id": int(caps.wMid) if status == MMSYSERR_NOERROR else None,
                "product_id": int(caps.wPid) if status == MMSYSERR_NOERROR else None,
                "technology": int(caps.wTechnology) if status == MMSYSERR_NOERROR else None,
            }
        )
    return inputs, outputs


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--out-dir", required=True)
    args = parser.parse_args()

    out_dir = Path(args.out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)
    inputs, outputs = enumerate_midi()
    matching_inputs = [device for device in inputs if device["matches_audio8dj"]]
    matching_outputs = [device for device in outputs if device["matches_audio8dj"]]
    summary = {
        "pass_enumeration": True,
        "input_device_count": len(inputs),
        "output_device_count": len(outputs),
        "matching_input_count": len(matching_inputs),
        "matching_output_count": len(matching_outputs),
        "midi_ready": len(matching_inputs) > 0 and len(matching_outputs) > 0,
        "matching_inputs": matching_inputs,
        "matching_outputs": matching_outputs,
    }
    (out_dir / "midi-devices.json").write_text(
        json.dumps({"inputs": inputs, "outputs": outputs}, indent=2),
        encoding="utf-8",
    )
    (out_dir / "summary.json").write_text(json.dumps(summary, indent=2), encoding="utf-8")
    print(json.dumps(summary, indent=2))


if __name__ == "__main__":
    main()
