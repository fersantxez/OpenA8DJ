#include <portcls.h>

#include "OpenA8DJMidiBusContract.h"
#include "OpenA8DJMidiBusInterface.h"
#include "OpenA8DJMidiMiniport.h"

#define OPENA8DJ_MIDI_MAX_MINIPORTS 1u

/* Inspectable in a kernel dump if StartDevice is blocked by a bad provider. */
volatile LONG g_OpenA8DJMidiQueryInterfaceCheckpoint =
    OPENA8DJ_MIDI_CHILD_QUERY_CHECKPOINT_NOT_STARTED;

static NTSTATUS
OpenA8DJMidiQueryBusInterface(
    _In_ PDEVICE_OBJECT physicalDeviceObject,
    _Out_ POPENA8DJ_MIDI_BUS_INTERFACE_V1 busInterface)
{
    KEVENT event;
    IO_STATUS_BLOCK ioStatus;
    PIRP irp;
    PIO_STACK_LOCATION stack;
    NTSTATUS status;

    if (physicalDeviceObject == nullptr || busInterface == nullptr) {
        return STATUS_INVALID_PARAMETER;
    }

    RtlZeroMemory(busInterface, sizeof(*busInterface));
    KeInitializeEvent(&event, NotificationEvent, FALSE);
    ioStatus.Status = STATUS_NOT_SUPPORTED;
    ioStatus.Information = 0u;
    irp = IoBuildSynchronousFsdRequest(
        IRP_MJ_PNP,
        physicalDeviceObject,
        nullptr,
        0u,
        nullptr,
        &event,
        &ioStatus);
    if (irp == nullptr) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    irp->IoStatus.Status = STATUS_NOT_SUPPORTED;
    stack = IoGetNextIrpStackLocation(irp);
    stack->MinorFunction = IRP_MN_QUERY_INTERFACE;
    stack->Parameters.QueryInterface.InterfaceType =
        &GUID_OPENA8DJ_MIDI_BUS_INTERFACE_V1;
    stack->Parameters.QueryInterface.Size = sizeof(*busInterface);
    stack->Parameters.QueryInterface.Version =
        OPENA8DJ_MIDI_BUS_INTERFACE_VERSION_V1;
    stack->Parameters.QueryInterface.Interface =
        reinterpret_cast<PINTERFACE>(busInterface);
    stack->Parameters.QueryInterface.InterfaceSpecificData = nullptr;

    InterlockedExchange(
        &g_OpenA8DJMidiQueryInterfaceCheckpoint,
        OPENA8DJ_MIDI_CHILD_QUERY_CHECKPOINT_CALLING_PARENT);
    status = IoCallDriver(physicalDeviceObject, irp);
    if (status == STATUS_PENDING) {
        InterlockedExchange(
            &g_OpenA8DJMidiQueryInterfaceCheckpoint,
            OPENA8DJ_MIDI_CHILD_QUERY_CHECKPOINT_PENDING_VIOLATION);
        DbgPrintEx(
            DPFLTR_IHVDRIVER_ID,
            DPFLTR_ERROR_LEVEL,
            "OpenA8DJMidi: parent violated synchronous QueryInterface contract; "
            "waiting for mandatory IRP completion\n");
        /*
         * Microsoft requires this wait for IoBuildSynchronousFsdRequest. The
         * event and IO_STATUS_BLOCK are stack-backed, so returning on a timeout
         * would create an unsafe use-after-return unless cancellation and final
         * completion ownership were first established. V1 therefore requires
         * the parent PDO to complete in dispatch; this wait is containment for
         * a provider bug and the checkpoint above identifies the exact stall.
         */
        status = KeWaitForSingleObject(
            &event,
            Executive,
            KernelMode,
            FALSE,
            nullptr);
        if (!NT_SUCCESS(status)) {
            return status;
        }
    }
    InterlockedExchange(
        &g_OpenA8DJMidiQueryInterfaceCheckpoint,
        OPENA8DJ_MIDI_CHILD_QUERY_CHECKPOINT_COMPLETED);
    return ioStatus.Status;
}

static ULONG
OpenA8DJMidiBusFunctionMask(
    _In_ const OPENA8DJ_MIDI_BUS_INTERFACE_V1 *busInterface)
{
    ULONG functions = 0u;

    if (busInterface->OpenStream != nullptr) {
        functions |= OPENA8DJ_MIDI_BUS_FUNCTION_OPEN;
    }
    if (busInterface->CloseStream != nullptr) {
        functions |= OPENA8DJ_MIDI_BUS_FUNCTION_CLOSE;
    }
    if (busInterface->SetState != nullptr) {
        functions |= OPENA8DJ_MIDI_BUS_FUNCTION_SET_STATE;
    }
    if (busInterface->ReadStream != nullptr) {
        functions |= OPENA8DJ_MIDI_BUS_FUNCTION_READ;
    }
    if (busInterface->WriteStream != nullptr) {
        functions |= OPENA8DJ_MIDI_BUS_FUNCTION_WRITE;
    }
    return functions;
}

static NTSTATUS
OpenA8DJMidiValidateQueriedInterface(
    _In_ const OPENA8DJ_MIDI_BUS_INTERFACE_V1 *busInterface)
{
    OPENA8DJ_MIDI_BUS_CONTRACT_VIEW contractView;
    OPENA8DJ_MIDI_BUS_CONTRACT_RESULT result;

    if (busInterface == nullptr || busInterface->Header.Context == nullptr ||
        busInterface->Header.InterfaceReference == nullptr ||
        busInterface->Header.InterfaceDereference == nullptr) {
        return STATUS_DEVICE_PROTOCOL_ERROR;
    }
    contractView.size = busInterface->Header.Size;
    contractView.requiredSize = sizeof(*busInterface);
    contractView.version = busInterface->Header.Version;
    contractView.reserved = 0u;
    contractView.magic = busInterface->Magic;
    contractView.capabilities = busInterface->Capabilities;
    contractView.functions = OpenA8DJMidiBusFunctionMask(busInterface);
    result = OpenA8DJMidiValidateBusContract(&contractView);
    if (result == OPENA8DJ_MIDI_BUS_CONTRACT_VERSION) {
        return STATUS_REVISION_MISMATCH;
    }
    if (result != OPENA8DJ_MIDI_BUS_CONTRACT_OK) {
        return STATUS_DEVICE_PROTOCOL_ERROR;
    }
    return STATUS_SUCCESS;
}

static VOID
OpenA8DJMidiDereferenceBusInterface(
    _Inout_ POPENA8DJ_MIDI_BUS_INTERFACE_V1 busInterface)
{
    if (busInterface->Header.InterfaceDereference != nullptr) {
        busInterface->Header.InterfaceDereference(
            busInterface->Header.Context);
        busInterface->Header.InterfaceDereference = nullptr;
    }
}

static NTSTATUS
OpenA8DJMidiStartDevice(
    _In_ PDEVICE_OBJECT deviceObject,
    _In_ PIRP irp,
    _In_ PRESOURCELIST resourceList)
{
    OPENA8DJ_MIDI_BUS_INTERFACE_V1 busInterface;
    PDEVICE_OBJECT physicalDeviceObject = nullptr;
    PPORT port = nullptr;
    PMINIPORTMIDI miniport = nullptr;
    NTSTATUS status;

    PAGED_CODE();

    status = PcGetPhysicalDeviceObject(deviceObject, &physicalDeviceObject);
    if (!NT_SUCCESS(status) || physicalDeviceObject == nullptr) {
        return NT_SUCCESS(status) ? STATUS_DEVICE_NOT_READY : status;
    }

    status = OpenA8DJMidiQueryBusInterface(
        physicalDeviceObject,
        &busInterface);
    if (!NT_SUCCESS(status)) {
        return status;
    }
    status = OpenA8DJMidiValidateQueriedInterface(&busInterface);
    if (!NT_SUCCESS(status)) {
        OpenA8DJMidiDereferenceBusInterface(&busInterface);
        return status;
    }
    InterlockedExchange(
        &g_OpenA8DJMidiQueryInterfaceCheckpoint,
        OPENA8DJ_MIDI_CHILD_QUERY_CHECKPOINT_VALIDATED);

    status = PcNewPort(&port, CLSID_PortMidi);
    if (NT_SUCCESS(status)) {
        status = OpenA8DJMidiCreateMiniport(&busInterface, &miniport);
        if (NT_SUCCESS(status)) {
            /* Factory consumed the query's one InterfaceReference. */
            RtlZeroMemory(&busInterface, sizeof(busInterface));
            status = port->Init(
                deviceObject,
                irp,
                miniport,
                nullptr,
                resourceList);
            if (NT_SUCCESS(status)) {
                status = PcRegisterSubdevice(
                    deviceObject,
                    const_cast<PWSTR>(L"OpenA8DJMidi"),
                    port);
            }
        }
    }

    if (miniport != nullptr) {
        miniport->Release();
    }
    if (port != nullptr) {
        port->Release();
    }
    OpenA8DJMidiDereferenceBusInterface(&busInterface);
    return status;
}

static NTSTATUS
OpenA8DJMidiAddDevice(
    _In_ PDRIVER_OBJECT driverObject,
    _In_ PDEVICE_OBJECT physicalDeviceObject)
{
    PAGED_CODE();

    return PcAddAdapterDevice(
        driverObject,
        physicalDeviceObject,
        OpenA8DJMidiStartDevice,
        OPENA8DJ_MIDI_MAX_MINIPORTS,
        0u);
}

extern "C"
DRIVER_INITIALIZE DriverEntry;

extern "C"
NTSTATUS
DriverEntry(
    _In_ PDRIVER_OBJECT driverObject,
    _In_ PUNICODE_STRING registryPath)
{
    return PcInitializeAdapterDriver(
        driverObject,
        registryPath,
        OpenA8DJMidiAddDevice);
}
