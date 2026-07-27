#!/usr/bin/env python3
"""Offline fixture and packaging contract tests for the hardware profiler."""

import argparse
import hashlib
import json
import re
import subprocess
import tempfile
from pathlib import Path


def check(condition, message):
    if not condition:
        raise AssertionError(message)


def invoke(binary, fixture, catalog, *extra):
    result = subprocess.run(
        [str(binary), "--json", "--fixture", str(fixture),
         "--catalog", str(catalog), *extra],
        text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
        timeout=5, check=False,
    )
    check(result.stdout.endswith("\n"), "JSON is not newline terminated")
    decoder = json.JSONDecoder()
    document, end = decoder.raw_decode(result.stdout)
    check(not result.stdout[end:].strip(), "more than one JSON document was emitted")
    check(document["schema"] == "org.opena8dj.hardware-profiler.report.v1",
          "wrong report schema")
    check(document["schemaVersion"] == 1, "wrong schema version type/value")
    check(result.returncode == document["overall"]["exitStatus"],
          "process exit and report overall exit disagree")
    return result, document


def codes(document):
    return {item["id"]: (item["status"], item["code"]) for item in document["checks"]}


def base_fixture():
    direction = {
        "samples": 20, "invalidIntervals": 0,
        "bins": {"le50": 20, "le100": 0, "le250": 0,
                 "le500": 0, "le1000": 0, "gt1000": 0},
    }
    envelope = {
        "schema": "org.opena8dj.public-api.response.v1", "apiVersion": "1.0",
        "ok": True,
    }
    iso = {
        "queueFailures": 0, "completionStatusFailures": 0,
        "transactionStatusFailures": 0, "zeroLengthTransactions": 0,
        "shortTransactions": 0,
    }
    return {
        "generatedAt": "2026-07-26T12:00:00Z",
        "osVersion": "15.5.0",
        "usbEnumerationAvailable": True,
        "usbCandidates": [{
            "vendorId": 0x17CC, "productId": 0x1978,
            "descriptorReadable": True, "currentConfiguration": 1, "usable": True,
            "bcdDevice": 257, "linkSpeedBitsPerSecond": 480_000_000,
            "requiredPowerMilliAmps": 500, "availablePowerMilliAmps": 500,
            "powerEvidenceScope": "device-current-pair",
            "powerEvidenceUnit": "mA",
            "failedRequestedPower": False,
        }],
        "coreAudioAvailable": True,
        "coreAudioDevices": [{"uid": "org.opena8dj.Audio8DJ",
                              "name": "Open Audio 8 DJ", "manufacturer": "OpenA8DJ"}],
        "api": {
            "provenance": "fixture",
            "version": {"state": "ok", "document": {
                **envelope, "operation": "version.get",
                "data": {"apiVersion": "1.0",
                         "schema": "org.opena8dj.public-api.response.v1",
                         "capabilities": [
                    "stats.read", "usb-quality.read", "hardware.read"
                ]},
            }},
            "hardware": {"state": "ok", "document": {
                **envelope, "operation": "hardware.get",
                "data": {"deviceInfoAvailable": True, "firmwareVersion": 31,
                         "hardwareSubtype": 0,
                         "capabilities": {"analogAudioOutputs": 8,
                                          "analogAudioInputs": 8,
                                          "digitalAudioOutputs": 0,
                                          "digitalAudioInputs": 0,
                                          "midiOutputs": 1, "midiInputs": 1,
                                          "dataAlignment": 2}},
            }},
            "stats": {"state": "ok", "document": {
                **envelope, "operation": "stats.get",
                "data": {
                    "stream": {"streaming": True, "sampleRate": 48000},
                    "capture": {"transfers": 20}, "playback": {"transfers": 20},
                    "output": {"underruns": 0, "activeUnderruns": 0,
                               "ringOverruns": 0},
                    "quality": {"instrumentationAvailable": True,
                                "completionJitter": {"capture": direction,
                                                     "playback": direction},
                                "isoErrors": {
                                    "capture": iso,
                                    "playback": iso,
                                }},
                },
            }},
        },
        "collectorErrors": [],
    }


def write(path, value):
    path.write_text(json.dumps(value), encoding="utf-8")


def assert_case(binary, directory, catalog, name, mutate, expected):
    fixture = base_fixture()
    mutate(fixture)
    path = directory / f"{name}.json"
    write(path, fixture)
    _, document = invoke(binary, path, catalog)
    actual = codes(document)
    for check_id, pair in expected.items():
        check(actual.get(check_id) == pair,
              f"{name}: {check_id} expected {pair}, got {actual.get(check_id)}")
    return document


def run(repo):
    test_binary = repo / "build/opena8dj-hardware-profiler-test"
    shipping = repo / "build/opena8dj-hardware-profiler"
    bundled = repo / "resources/hardware-profiler-known-issues-v1.json"
    with tempfile.TemporaryDirectory(prefix="a8profiler-", dir="/tmp") as temporary:
        directory = Path(temporary)
        recognized = json.loads(bundled.read_text())
        recognized["recognizedFirmwareVersions"] = [31]
        catalog = directory / "catalog.json"
        write(catalog, recognized)

        assert_case(test_binary, directory, catalog, "absent",
                    lambda f: f["usbCandidates"].clear(),
                    {"usb.identity": ("FAIL", "USB_DEVICE_NOT_FOUND")})
        assert_case(test_binary, directory, catalog, "usb-query-unavailable",
                    lambda f: f.update(usbEnumerationAvailable=False),
                    {"usb.identity": ("UNKNOWN", "USB_IDENTITY_UNKNOWN"),
                     "usb.enumeration": ("UNKNOWN", "USB_ENUMERATION_UNKNOWN")})
        assert_case(test_binary, directory, catalog, "mismatch",
                    lambda f: f.update(usbCandidates=[{"vendorId": 1, "productId": 2}]),
                    {"usb.identity": ("FAIL", "USB_IDENTITY_MISMATCH")})
        assert_case(test_binary, directory, catalog, "recognized", lambda f: None,
                    {"device.firmware": ("PASS", "DEVICE_FIRMWARE_RECOGNIZED")})
        assert_case(test_binary, directory, catalog, "unknown-firmware",
                    lambda f: f["api"]["hardware"]["document"]["data"].update(
                        firmwareVersion=999),
                    {"device.firmware": ("WARN", "DEVICE_FIRMWARE_UNKNOWN")})
        assert_case(test_binary, directory, catalog, "power-unknown",
                    lambda f: [f["usbCandidates"][0].pop(key) for key in
                               ("requiredPowerMilliAmps", "availablePowerMilliAmps")],
                    {"usb.power": ("UNKNOWN", "USB_POWER_UNKNOWN")})
        assert_case(test_binary, directory, catalog, "power-low",
                    lambda f: f["usbCandidates"][0].update(
                        requiredPowerMilliAmps=500, availablePowerMilliAmps=100),
                    {"usb.power": ("FAIL", "USB_POWER_INSUFFICIENT")})
        assert_case(test_binary, directory, catalog, "power-scope-unknown",
                    lambda f: f["usbCandidates"][0].pop("powerEvidenceScope"),
                    {"usb.power": ("UNKNOWN", "USB_POWER_UNKNOWN")})
        assert_case(test_binary, directory, catalog, "power-flag",
                    lambda f: f["usbCandidates"][0].update(failedRequestedPower=True),
                    {"usb.power": ("FAIL", "USB_POWER_INSUFFICIENT")})
        assert_case(test_binary, directory, catalog, "api-down",
                    lambda f: f["api"].update(hardware={"state": "unavailable"}),
                    {"driver.api-pairing": ("FAIL", "DRIVER_API_MISMATCH"),
                     "device.firmware": ("UNKNOWN", "DEVICE_INFO_UNAVAILABLE")})
        def incomplete_hardware(fixture):
            fixture["api"]["hardware"]["document"]["data"]["capabilities"].pop(
                "dataAlignment")
        assert_case(test_binary, directory, catalog, "device-info-incomplete",
                    incomplete_hardware,
                    {"device.firmware": ("FAIL", "DEVICE_INFO_INVALID"),
                     "driver.api-pairing": ("FAIL", "DRIVER_API_MISMATCH")})
        assert_case(test_binary, directory, catalog, "api-schema-wrong",
                    lambda f: f["api"]["stats"]["document"].update(schema="wrong"),
                    {"driver.api-pairing": ("FAIL", "DRIVER_API_MISMATCH"),
                     "usb.stream-quality": ("UNKNOWN", "USB_QUALITY_UNAVAILABLE")})
        assert_case(test_binary, directory, catalog, "quality-marker",
                    lambda f: f["api"]["stats"]["document"]["data"]["quality"].update(
                        instrumentationAvailable=False),
                    {"usb.stream-quality": ("UNKNOWN", "USB_QUALITY_UNAVAILABLE")})
        assert_case(test_binary, directory, catalog, "quality-invalid",
                    lambda f: f["api"]["stats"]["document"]["data"]["quality"][
                        "completionJitter"]["capture"]["bins"].update(le50=19),
                    {"usb.stream-quality": ("FAIL", "USB_QUALITY_INVALID")})
        assert_case(test_binary, directory, catalog, "quality-missing-iso",
                    lambda f: f["api"]["stats"]["document"]["data"]["quality"].pop(
                        "isoErrors"),
                    {"usb.stream-quality": ("FAIL", "USB_QUALITY_INVALID"),
                     "driver.api-pairing": ("FAIL", "DRIVER_API_MISMATCH")})
        assert_case(test_binary, directory, catalog, "quality-missing-output",
                    lambda f: f["api"]["stats"]["document"]["data"].pop("output"),
                    {"usb.stream-quality": ("FAIL", "USB_QUALITY_INVALID"),
                     "driver.api-pairing": ("FAIL", "DRIVER_API_MISMATCH")})
        assert_case(test_binary, directory, catalog, "quality-startup-underruns",
                    lambda f: f["api"]["stats"]["document"]["data"]["output"].update(
                        underruns=9),
                    {"usb.stream-quality": ("PASS", "USB_QUALITY_HEALTHY")})
        assert_case(test_binary, directory, catalog, "quality-active-underrun",
                    lambda f: f["api"]["stats"]["document"]["data"]["output"].update(
                        activeUnderruns=1),
                    {"usb.stream-quality": ("WARN", "USB_QUALITY_DEGRADED")})

        raw = base_fixture()
        raw["rawUSBRegistryProperties"] = [{
            "idVendor": 0x17CC, "idProduct": 0x1978, "bcdDevice": 257,
            "kUSBCurrentConfiguration": 1, "USBSpeed": 3,
            "USB Current Available": 500, "USB Current Required": 500,
            "kUSBFailedRequestedPower": False,
        }]
        raw_path = directory / "raw-usb.json"
        write(raw_path, raw)
        _, raw_document = invoke(test_binary, raw_path, catalog)
        check(codes(raw_document)["usb.enumeration"] ==
              ("PASS", "USB_ENUMERATION_OK") and
              codes(raw_document)["usb.link-speed"] ==
              ("PASS", "USB_LINK_HIGH_SPEED") and
              codes(raw_document)["usb.power"] ==
              ("PASS", "USB_POWER_SUFFICIENT"),
              "documented raw USB properties did not normalize correctly")

        ambiguous = base_fixture()
        ambiguous["rawUSBRegistryProperties"] = [{
            "idVendor": 0x17CC, "idProduct": 0x1978, "bcdDevice": 257,
            "bConfigurationValue": 1, "Device Speed": 480_000_000,
            "USBSpeed": 7, "UsbPowerSinkAllocation": 500,
            "Bus Power Available": 500, "USB Current Required": 500,
        }]
        ambiguous_path = directory / "ambiguous-usb.json"
        write(ambiguous_path, ambiguous)
        _, ambiguous_document = invoke(test_binary, ambiguous_path, catalog)
        check(codes(ambiguous_document)["usb.enumeration"] ==
              ("WARN", "USB_DESCRIPTOR_INCOMPLETE") and
              codes(ambiguous_document)["usb.link-speed"] ==
              ("UNKNOWN", "USB_LINK_UNKNOWN") and
              codes(ambiguous_document)["usb.power"] ==
              ("UNKNOWN", "USB_POWER_UNKNOWN"),
              "ambiguous USB enum/power/configuration keys were trusted")

        privacy_fixture = base_fixture()
        secrets = ["/Users/alice/private.sock", "SERIAL-SECRET", "0x1234abcd",
                   "alice", "secret-registry-path"]
        privacy_fixture.update(usbSerial=secrets[1], usbLocationId=secrets[2],
                               userName=secrets[3], registryPath=secrets[4],
                               generatedAt=secrets[0])
        privacy_fixture["collectorErrors"] = [{
            "source": "fixture", "reasonCode": "redacted",
            "message": secrets[0]
        }]
        privacy_path = directory / "privacy.json"
        write(privacy_path, privacy_fixture)
        result, privacy_document = invoke(test_binary, privacy_path, catalog)
        human = subprocess.run(
            [str(test_binary), "--fixture", str(privacy_path), "--catalog", str(catalog)],
            text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=False, timeout=5)
        for secret in secrets:
            check(secret not in result.stdout + result.stderr + human.stdout + human.stderr,
                  f"privacy secret leaked: {secret}")
        human_codes = {
            match.group(2): (match.group(1), match.group(3))
            for match in re.finditer(
                r"^\[(PASS|WARN|FAIL|UNKNOWN)\] ([^ ]+) ([A-Z0-9_]+):",
                human.stdout, re.M)
        }
        json_codes = {
            item["id"]: (item["status"], item["code"])
            for item in privacy_document["checks"]
        }
        check(human_codes == json_codes, "human and JSON status/code semantics differ")

        invalid_catalog = directory / "invalid-catalog.json"
        duplicate = json.loads(bundled.read_text())
        rule = {"id": "duplicate", "status": "WARN", "summary": "Documented.",
                "source": {"title": "Test", "version": "1"},
                "all": [{"fact": "usb.vendorId", "op": "eq", "value": 6092}],
                "remediation": ["Test remediation."], "exclusiveGroup": None}
        duplicate["issues"] = [rule, rule]
        write(invalid_catalog, duplicate)
        fixture_path = directory / "base.json"
        write(fixture_path, base_fixture())
        _, document = invoke(test_binary, fixture_path, invalid_catalog)
        check(codes(document)["known-issues.catalog"] ==
              ("UNKNOWN", "KNOWN_ISSUES_CATALOG_INVALID"), "duplicate rules accepted")

        invalid_group_catalog = directory / "invalid-group.json"
        invalid_group = json.loads(bundled.read_text())
        invalid_group["issues"] = [{**rule, "id": "bad-group", "exclusiveGroup": 7}]
        write(invalid_group_catalog, invalid_group)
        _, document = invoke(test_binary, fixture_path, invalid_group_catalog)
        check(codes(document)["known-issues.catalog"] ==
              ("UNKNOWN", "KNOWN_ISSUES_CATALOG_INVALID"),
              "non-string exclusiveGroup was accepted")
        invalid_group["issues"][0]["exclusiveGroup"] = ""
        write(invalid_group_catalog, invalid_group)
        _, document = invoke(test_binary, fixture_path, invalid_group_catalog)
        check(codes(document)["known-issues.catalog"] ==
              ("UNKNOWN", "KNOWN_ISSUES_CATALOG_INVALID"),
              "empty exclusiveGroup was accepted")
        invalid_group["issues"][0]["exclusiveGroup"] = None
        invalid_group["issues"][0]["summary"] = "x" * 1025
        write(invalid_group_catalog, invalid_group)
        _, document = invoke(test_binary, fixture_path, invalid_group_catalog)
        check(codes(document)["known-issues.catalog"] ==
              ("UNKNOWN", "KNOWN_ISSUES_CATALOG_INVALID"),
              "oversized catalog field was accepted")

        conflict_catalog = directory / "conflict.json"
        fail_rule = dict(rule)
        fail_rule.update(id="conflict-fail", status="FAIL",
                         remediation=["Different."], exclusiveGroup="one")
        warn_rule = dict(rule)
        warn_rule.update(id="conflict-warn", exclusiveGroup="one")
        conflict = json.loads(bundled.read_text())
        conflict["issues"] = [warn_rule, fail_rule]
        write(conflict_catalog, conflict)
        _, document = invoke(test_binary, fixture_path, conflict_catalog)
        check(codes(document)["known-issues.catalog"] ==
              ("UNKNOWN", "KNOWN_ISSUES_CATALOG_CONFLICT"), "conflict selected by order")

        unsupported_catalog = directory / "unsupported.json"
        unsupported = json.loads(bundled.read_text())
        unsupported["recognizedFirmwareVersions"] = [31]
        unsupported["issues"] = [{
            **rule, "id": "unsupported-firmware", "status": "FAIL",
            "all": [{"fact": "device.firmwareVersion", "op": "eq", "value": 31}],
        }]
        write(unsupported_catalog, unsupported)
        _, document = invoke(test_binary, fixture_path, unsupported_catalog)
        check(codes(document)["device.firmware"] == ("FAIL", "DEVICE_INFO_INVALID") and
              codes(document)["known-issues.catalog"] ==
              ("FAIL", "KNOWN_ISSUE_UNSUPPORTED"),
              "explicit unsupported firmware did not fail both checks")

        unresolved_catalog = directory / "unresolved.json"
        unresolved = json.loads(bundled.read_text())
        unresolved["recognizedFirmwareVersions"] = [31]
        unresolved["issues"] = [{
            **rule, "id": "missing-firmware",
            "all": [{"fact": "usb.vendorId", "op": "eq", "value": 6092},
                    {"fact": "device.firmwareVersion", "op": "eq", "value": 31}],
        }]
        write(unresolved_catalog, unresolved)
        missing = base_fixture()
        missing_data = missing["api"]["hardware"]["document"]["data"]
        missing_data["deviceInfoAvailable"] = False
        missing_data["firmwareVersion"] = None
        missing_data["hardwareSubtype"] = None
        missing_data["capabilities"] = {
            key: None for key in missing_data["capabilities"]
        }
        missing_path = directory / "missing.json"
        write(missing_path, missing)
        _, document = invoke(test_binary, missing_path, unresolved_catalog)
        check(codes(document)["known-issues.catalog"] ==
              ("UNKNOWN", "KNOWN_ISSUES_EVIDENCE_MISSING"), "missing evidence unmatched")
        check(codes(document)["device.firmware"] ==
              ("UNKNOWN", "DEVICE_INFO_UNAVAILABLE"),
              "recognized catalog fabricated absent firmware evidence")

        rejected = subprocess.run(
            [str(shipping), "--fixture", str(fixture_path)],
            text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
            timeout=5, check=False)
        check(rejected.returncode == 64 and rejected.stdout == "",
              "shipping binary recognized --fixture")

        _, missing_catalog_document = invoke(
            test_binary, fixture_path, directory / "does-not-exist.json")
        check(codes(missing_catalog_document)["known-issues.catalog"] ==
              ("UNKNOWN", "KNOWN_ISSUES_CATALOG_INVALID"),
              "missing catalog became an internal error")

    source = (repo / "src/tools/opena8dj-hardware-profiler.swift").read_text()
    check("ProcessInfo.processInfo.environment" not in source,
          "fixture or control routing reads environment")
    check("GET_DEVICE_INFO" not in source and "IOUSBHostDevice(" not in source,
          "profiler contains a direct device open/query path")
    check("expectedVID = 0x17cc" in source and "expectedPID = 0x1978" in source,
          "exact identity constants changed")
    check("productName" not in source, "name-based USB fallback exists")
    check('"kUSBCurrentConfiguration"' in source and
          '"bConfigurationValue"' not in source,
          "current configuration does not use the IOUSBHost current key")
    for forbidden in ['"Device Speed"', '"USB Link Speed"',
                      '"UsbPowerSinkAllocation"', '"kUSBBusCurrentAllocation"',
                      '"Bus Power Available"', '"Current Available"']:
        check(forbidden not in source, f"ambiguous live USB property remains: {forbidden}")
    check('"UsbLinkSpeed"' in source and '"USBSpeed"' in source and
          "connectionSpeedBitsPerSecond" in source,
          "USB bitrate/enum normalization is missing")
    check("SIGTERM" in source and "SIGKILL" in source and
          "waitUntilExit()" in source and "DispatchGroup" in source,
          "child timeout does not terminate, kill, reap, and join readers")

    makefile = (repo / "Makefile").read_text()
    catalog_bytes = bundled.read_bytes()
    check("opena8dj-hardware-profiler" in makefile and
          "hardware-profiler-known-issues-v1.json" in makefile,
          "package roots do not include profiler/catalog")
    check(hashlib.sha256(catalog_bytes).hexdigest() ==
          hashlib.sha256((repo / "build/hardware-profiler-known-issues-v1.json").read_bytes()).hexdigest(),
          "built catalog bytes differ")
    for script in [
        "resources/pkg/scripts/preinstall",
        "resources/pkg/scripts/uninstall-opena8dj.sh",
        "resources/control-surfaces-pkg/scripts/preinstall",
        "resources/control-surfaces-pkg/scripts/uninstall-opena8dj-control-surfaces.sh",
    ]:
        text = (repo / script).read_text()
        check("opena8dj-hardware-profiler" in text and
              "hardware-profiler-known-issues-v1.json" in text,
              f"{script} does not remove profiler/catalog")

    hal = (repo / "src/hal/OpenA8DJUSB.m").read_text()
    cli = (repo / "src/tools/opena8dj-control.c").read_text()
    pattern = re.compile(
        r"typedef struct OpenA8DJStreamStatsPayload \{(.*?)\}"
        r" __attribute__\(\(packed\)\) OpenA8DJStreamStatsPayload;", re.S)
    hal_fields = re.findall(r"\b(?:uint8_t|uint32_t|uint64_t|double)\s+(\w+);",
                            pattern.search(hal).group(1))
    cli_fields = re.findall(r"\b(?:uint8_t|uint32_t|uint64_t|double)\s+(\w+);",
                            pattern.search(cli).group(1))
    expected_tail = [
        "deviceInfoAvailable", "deviceFirmwareVersion", "deviceHardwareSubtype",
        "deviceNumAnalogAudioOut", "deviceNumAnalogAudioIn",
        "deviceNumDigitalAudioOut", "deviceNumDigitalAudioIn",
        "deviceNumMidiOut", "deviceNumMidiIn", "deviceDataAlignment",
    ]
    check(hal_fields == cli_fields and hal_fields[-10:] == expected_tail,
          "append-only payload parity/tail failed")
    check("_deviceInfoAvailable = true" in hal and
          "bytes[0] != kCommandGetDeviceInfo" in hal,
          "cached device marker is not command-qualified")
    print("hardware profiler offline tests: PASS")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo", type=Path, required=True)
    args = parser.parse_args()
    run(args.repo.resolve())


if __name__ == "__main__":
    main()
