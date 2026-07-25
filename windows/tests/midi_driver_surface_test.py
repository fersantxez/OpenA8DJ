from __future__ import annotations

import pathlib
import sys
import xml.etree.ElementTree as ET


ROOT = pathlib.Path(__file__).resolve().parents[2]
DRIVER = ROOT / "windows" / "midi" / "driver"


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def text(name: str) -> str:
    return (DRIVER / name).read_text(encoding="utf-8")


def between(source: str, start: str, end: str) -> str:
    begin = source.index(start)
    finish = source.index(end, begin)
    return source[begin:finish]


def main() -> int:
    inf = text("OpenA8DJMidi.inf")
    project_path = DRIVER / "OpenA8DJMidi.vcxproj"
    project = project_path.read_text(encoding="utf-8")
    adapter = text("OpenA8DJMidiAdapter.cpp")
    interface = text("OpenA8DJMidiBusInterface.h")
    contract = text("OpenA8DJMidiBusContract.h")
    miniport = text("OpenA8DJMidiMiniport.cpp")
    descriptors = text("OpenA8DJMidiDescriptors.cpp")
    miniport_header = text("OpenA8DJMidiMiniport.h")

    require("Class=MEDIA" in inf, "INF must use MEDIA class")
    require("OPENA8DJ\\MIDI" in inf, "INF must bind only to child PDO ID")
    require("StartType=3" in inf, "driver must remain demand-start")
    require("OpenA8DJMidi.sys" in inf, "INF service binary missing")
    require("MidiSrv" not in inf + project, "Windows MIDI Services dependency found")
    require("OpenA8DJUsb" not in inf, "MIDI package must not bind physical USB")
    require(inf.count("MidiReference=\"OpenA8DJMidi\"") == 1,
            "PortCls reference string must be stable")

    tree = ET.parse(project_path)
    namespace = {"m": "http://schemas.microsoft.com/developer/msbuild/2003"}
    compiled = {
        element.attrib["Include"]
        for element in tree.findall(".//m:ClCompile", namespace)
        if "Include" in element.attrib
    }
    require("OpenA8DJMidiAdapter.cpp" in compiled, "adapter not compiled")
    require("OpenA8DJMidiMiniport.cpp" in compiled, "miniport not compiled")
    require("OpenA8DJMidiBusContract.c" in compiled, "ABI validator not compiled")
    require("<WarningLevel>Level4</WarningLevel>" in project, "missing /W4")
    require("<TreatWarningAsError>true</TreatWarningAsError>" in project,
            "missing /WX")
    require("<EnableTestSign>false</EnableTestSign>" in project,
            "automatic WDK test signing must stay disabled")
    require("<SignMode>Off</SignMode>" in project,
            "driver signing must stay disabled for offline slices")
    require("<SupportsPackaging>false</SupportsPackaging>" in project,
            "offline project must not package automatically")
    require("NTDDI_VERSION=NTDDI_WIN10_VB" in project,
            "driver must target Windows 10 2004 or newer")
    require("Include=\"Release|x64\"" in project and "Win32" not in project,
            "S1 scope must remain x64 only")
    require("NTamd64.10.0...19041" in inf,
            "INF minimum build must remain Windows 10 2004")

    query = adapter.index("OpenA8DJMidiQueryBusInterface")
    validate = adapter.index("OpenA8DJMidiValidateQueriedInterface")
    register = adapter.index("PcRegisterSubdevice")
    require(query < register and validate < register,
            "bus query/validation must precede PortCls registration")
    require("IRP_MN_QUERY_INTERFACE" in adapter, "bus query IRP missing")
    require("STATUS_REVISION_MISMATCH" in adapter, "version fail-closed missing")
    require("InterfaceDereference" in adapter + miniport,
            "bus interface lifetime release missing")
    require("g_OpenA8DJMidiQueryInterfaceCheckpoint" in adapter,
            "query-interface diagnostic checkpoint missing")
    require("OPENA8DJ_MIDI_CHILD_QUERY_CHECKPOINT_PENDING_VIOLATION" in adapter,
            "pending QueryInterface violation checkpoint missing")
    require("OPENA8DJ_MIDI_BUS_CAP_SYNCHRONOUS_QUERY" in contract,
            "synchronous PDO query capability missing")
    for checkpoint in (
        "OPENA8DJ_MIDI_PARENT_QUERY_CHECKPOINT_ENTERED",
        "OPENA8DJ_MIDI_PARENT_QUERY_CHECKPOINT_REFERENCED",
        "OPENA8DJ_MIDI_PARENT_QUERY_CHECKPOINT_COMPLETED",
    ):
        require(checkpoint in contract, f"parent checkpoint missing: {checkpoint}")
    require("KeWaitForSingleObject" in adapter and "IoCancelIrp" not in adapter,
            "QueryInterface must use the official synchronous completion pattern")
    require("timeout cannot safely" in interface and
            "complete IRP_MN_QUERY_INTERFACE synchronously" in interface,
            "safe no-timeout rationale/provider contract missing")

    for function in ("OpenStream", "CloseStream", "SetState", "ReadStream", "WriteStream"):
        require(function in interface, f"bus operation missing: {function}")
    require("OPENA8DJ_MIDI_BUS_CAP_NONBLOCKING_IO" in contract,
            "nonblocking transport capability missing")
    require("OPENA8DJ_MIDI_BUS_CAP_ATOMIC_WRITE" in contract,
            "atomic write capability missing")
    require("OPENA8DJ_MIDI_MINIMUM_WINDOWS_BUILD 19041u" in contract,
            "Windows 10 2004 scope constant missing")
    require("OPENA8DJ_MIDI_BUS_INTERFACE_V1_X64_SIZE 80u" in contract,
            "x64 ABI size constant missing")
    require("KSDATAFORMAT_TYPE_MUSIC" in descriptors, "KS music format missing")
    require("KSDATAFORMAT_SUBTYPE_MIDI" in descriptors, "MIDI subtype missing")
    require("KSMUSIC_TECHNOLOGY_PORT" in descriptors, "MIDI port technology missing")
    require("STATICGUIDOF(KSMUSIC_TECHNOLOGY_PORT),\n    0u,\n    0u,\n    0xffffu" in descriptors,
            "MIDI port range must set Channels/Notes=0 and ChannelMask=0xffff")
    require("OpenA8DJMidiIsSupportedDataRange" in descriptors + miniport,
            "extended KSDATARANGE_MUSIC validation missing")
    for field in ("musicRange->Channels == 0u",
                  "musicRange->Notes == 0u",
                  "musicRange->ChannelMask == 0xffffu"):
        require(field in descriptors, f"extended music range field not validated: {field}")
    require("KSPIN_DATAFLOW_IN" in descriptors and "KSPIN_DATAFLOW_OUT" in descriptors,
            "duplex MIDI pins missing")
    require("IMiniportMidi" in miniport and "IMiniportMidiStream" in miniport,
            "PortCls MIDI interfaces missing")
    require("PcNewServiceGroup" in miniport and "Notify(m_ServiceGroup)" in miniport,
            "PortCls MIDI service-group notification path missing")

    new_stream = between(miniport,
                         "COpenA8DJMidiMiniport::NewStream(",
                         "COpenA8DJMidiMiniport::BusInterface()")
    create = new_stream.index("COpenA8DJMidiStream::Create")
    add_ref = new_stream.index("m_ServiceGroup->AddRef()", create)
    output = new_stream.index("*serviceGroup = m_ServiceGroup", add_ref)
    require(create < add_ref < output,
            "NewStream must AddRef and return the canonical ServiceGroup only after stream success")
    require("if (!NT_SUCCESS(status))" in new_stream,
            "NewStream failure unwind missing")

    set_state = between(miniport,
                        "COpenA8DJMidiStream::SetState(",
                        "COpenA8DJMidiStream::Read(")
    publish = set_state.index("InterlockedExchange(&m_State")
    notify = set_state.index("m_Miniport->NotifyPort()", publish)
    require(publish < notify and "state == KSSTATE_RUN" in set_state,
            "capture RUN must publish state before forced PortCls notify")

    write = between(miniport,
                    "COpenA8DJMidiStream::Write(",
                    "OpenA8DJMidiCreateMiniport(")
    require("status == STATUS_DEVICE_BUSY" in write,
            "parent backpressure result is not recognized")
    require("OPENA8DJ_MIDI_PORTCLS_WRITE_BACKPRESSURE" in write and
            "return STATUS_SUCCESS" in write,
            "backpressure must map to PortCls success with zero bytes")
    require("OPENA8DJ_MIDI_PORTCLS_WRITE_PROTOCOL_ERROR" in write and
            "STATUS_DEVICE_PROTOCOL_ERROR" in write,
            "partial V1 writes must fail as protocol errors")

    require("consumes exactly the one" in miniport_header and
            "Factory consumed the query's one InterfaceReference" in adapter,
            "single InterfaceReference ownership transfer is undocumented")
    transfer = between(adapter,
                       "OpenA8DJMidiCreateMiniport(&busInterface",
                       "port->Init(")
    require(transfer.index("OpenA8DJMidiCreateMiniport") <
            transfer.index("RtlZeroMemory(&busInterface"),
            "caller ownership must transfer only after factory success")

    forbidden = ("WdfUsb", "WDFUSBPIPE", "WinUsb", "CreateFile(", "SetupDi")
    combined = adapter + interface + miniport + descriptors
    for token in forbidden:
        require(token not in combined, f"hardware or user-mode dependency found: {token}")

    print("PASS: OpenA8DJ MIDI driver offline surface contract")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except AssertionError as error:
        print(f"FAIL: {error}", file=sys.stderr)
        raise SystemExit(1)
