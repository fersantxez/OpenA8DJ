#include "OpenA8DJMidiMiniport.h"

#include "OpenA8DJMidiDescriptors.h"

#define OPENA8DJ_MIDI_MINIPORT_POOL_TAG 0x4d38414fu
#define OPENA8DJ_MIDI_STREAM_POOL_TAG 0x5338414fu

void * __cdecl
operator new(size_t size, void *placement) noexcept
{
    UNREFERENCED_PARAMETER(size);
    return placement;
}

void __cdecl
operator delete(void *object, void *placement) noexcept
{
    UNREFERENCED_PARAMETER(object);
    UNREFERENCED_PARAMETER(placement);
}

void __cdecl
operator delete(void *object, size_t size) noexcept
{
    UNREFERENCED_PARAMETER(size);
    if (object != nullptr) {
        ExFreePool(object);
    }
}

class COpenA8DJMidiMiniport final : public IMiniportMidi
{
public:
    explicit COpenA8DJMidiMiniport(
        _In_ const OPENA8DJ_MIDI_BUS_INTERFACE_V1 *busInterface) noexcept;

    STDMETHODIMP QueryInterface(_In_ REFIID interfaceId, _Out_ PVOID *object) override;
    STDMETHODIMP_(ULONG) AddRef() override;
    STDMETHODIMP_(ULONG) Release() override;

    STDMETHODIMP GetDescription(_Out_ PPCFILTER_DESCRIPTOR *description) override;
    STDMETHODIMP DataRangeIntersection(
        _In_ ULONG pinId,
        _In_ PKSDATARANGE dataRange,
        _In_ PKSDATARANGE matchingDataRange,
        _In_ ULONG outputBufferLength,
        _Out_writes_bytes_to_opt_(outputBufferLength, *resultantFormatLength)
            PVOID resultantFormat,
        _Out_ PULONG resultantFormatLength) override;
    STDMETHODIMP Init(
        _In_ PUNKNOWN unknownAdapter,
        _In_ PRESOURCELIST resourceList,
        _In_ PPORTMIDI port,
        _Out_ PSERVICEGROUP *serviceGroup) override;
    STDMETHODIMP_(void) Service() override;
    STDMETHODIMP NewStream(
        _Out_ PMINIPORTMIDISTREAM *stream,
        _In_opt_ PUNKNOWN outerUnknown,
        _In_ POOL_TYPE poolType,
        _In_ ULONG pin,
        _In_ BOOLEAN capture,
        _In_ PKSDATAFORMAT dataFormat,
        _Out_ PSERVICEGROUP *serviceGroup) override;

    const OPENA8DJ_MIDI_BUS_INTERFACE_V1 &BusInterface() const noexcept;
    void NotifyPort() noexcept;

private:
    ~COpenA8DJMidiMiniport();

    volatile LONG m_ReferenceCount;
    OPENA8DJ_MIDI_BUS_INTERFACE_V1 m_BusInterface;
    PPORTMIDI m_Port;
    PSERVICEGROUP m_ServiceGroup;
};

class COpenA8DJMidiStream final : public IMiniportMidiStream
{
public:
    static NTSTATUS Create(
        _In_ COpenA8DJMidiMiniport *miniport,
        _In_ BOOLEAN capture,
        _Out_ PMINIPORTMIDISTREAM *stream);

    STDMETHODIMP QueryInterface(_In_ REFIID interfaceId, _Out_ PVOID *object) override;
    STDMETHODIMP_(ULONG) AddRef() override;
    STDMETHODIMP_(ULONG) Release() override;
    STDMETHODIMP SetFormat(_In_ PKSDATAFORMAT dataFormat) override;
    STDMETHODIMP SetState(_In_ KSSTATE state) override;
    STDMETHODIMP Read(
        _In_ PVOID bufferAddress,
        _In_ ULONG bufferLength,
        _Out_ PULONG bytesRead) override;
    STDMETHODIMP Write(
        _In_ PVOID bufferAddress,
        _In_ ULONG bytesToWrite,
        _Out_ PULONG bytesWritten) override;

private:
    COpenA8DJMidiStream(
        _In_ COpenA8DJMidiMiniport *miniport,
        _In_ BOOLEAN capture) noexcept;
    ~COpenA8DJMidiStream();

    NTSTATUS Open();
    static VOID NTAPI BusNotify(_In_ PVOID notificationContext);

    volatile LONG m_ReferenceCount;
    COpenA8DJMidiMiniport *m_Miniport;
    PVOID m_BusStreamContext;
    volatile LONG m_State;
    BOOLEAN m_Capture;
};

COpenA8DJMidiMiniport::COpenA8DJMidiMiniport(
    _In_ const OPENA8DJ_MIDI_BUS_INTERFACE_V1 *busInterface) noexcept
    : m_ReferenceCount(1),
      m_BusInterface(*busInterface),
      m_Port(nullptr),
      m_ServiceGroup(nullptr)
{
}

COpenA8DJMidiMiniport::~COpenA8DJMidiMiniport()
{
    if (m_Port != nullptr) {
        m_Port->Release();
        m_Port = nullptr;
    }
    if (m_ServiceGroup != nullptr) {
        m_ServiceGroup->Release();
        m_ServiceGroup = nullptr;
    }
    if (m_BusInterface.Header.InterfaceDereference != nullptr) {
        m_BusInterface.Header.InterfaceDereference(
            m_BusInterface.Header.Context);
        m_BusInterface.Header.InterfaceDereference = nullptr;
    }
}

STDMETHODIMP
COpenA8DJMidiMiniport::QueryInterface(
    _In_ REFIID interfaceId,
    _Out_ PVOID *object)
{
    if (object == nullptr) {
        return STATUS_INVALID_PARAMETER;
    }
    *object = nullptr;
    if (IsEqualGUIDAligned(interfaceId, IID_IUnknown) ||
        IsEqualGUIDAligned(interfaceId, IID_IMiniport) ||
        IsEqualGUIDAligned(interfaceId, IID_IMiniportMidi)) {
        *object = static_cast<PMINIPORTMIDI>(this);
        AddRef();
        return STATUS_SUCCESS;
    }
    return STATUS_NOINTERFACE;
}

STDMETHODIMP_(ULONG)
COpenA8DJMidiMiniport::AddRef()
{
    return static_cast<ULONG>(InterlockedIncrement(&m_ReferenceCount));
}

STDMETHODIMP_(ULONG)
COpenA8DJMidiMiniport::Release()
{
    ULONG referenceCount =
        static_cast<ULONG>(InterlockedDecrement(&m_ReferenceCount));

    if (referenceCount == 0u) {
        this->~COpenA8DJMidiMiniport();
        ExFreePoolWithTag(this, OPENA8DJ_MIDI_MINIPORT_POOL_TAG);
    }
    return referenceCount;
}

STDMETHODIMP
COpenA8DJMidiMiniport::GetDescription(
    _Out_ PPCFILTER_DESCRIPTOR *description)
{
    if (description == nullptr) {
        return STATUS_INVALID_PARAMETER;
    }
    *description =
        const_cast<PPCFILTER_DESCRIPTOR>(&g_OpenA8DJMidiFilterDescriptor);
    return STATUS_SUCCESS;
}

STDMETHODIMP
COpenA8DJMidiMiniport::DataRangeIntersection(
    _In_ ULONG pinId,
    _In_ PKSDATARANGE dataRange,
    _In_ PKSDATARANGE matchingDataRange,
    _In_ ULONG outputBufferLength,
    _Out_writes_bytes_to_opt_(outputBufferLength, *resultantFormatLength)
        PVOID resultantFormat,
    _Out_ PULONG resultantFormatLength)
{
    if (resultantFormatLength == nullptr) {
        return STATUS_INVALID_PARAMETER;
    }
    *resultantFormatLength = sizeof(KSDATAFORMAT);

    if (pinId >= OPENA8DJ_MIDI_PIN_COUNT || dataRange == nullptr ||
        matchingDataRange == nullptr ||
        !OpenA8DJMidiIsSupportedFormat(dataRange) ||
        (dataRange->FormatSize != sizeof(KSDATARANGE) &&
         !OpenA8DJMidiIsSupportedDataRange(dataRange)) ||
        !OpenA8DJMidiIsSupportedDataRange(matchingDataRange)) {
        return STATUS_NO_MATCH;
    }
    if (outputBufferLength == 0u) {
        return STATUS_BUFFER_OVERFLOW;
    }
    if (outputBufferLength < sizeof(KSDATAFORMAT)) {
        return STATUS_BUFFER_TOO_SMALL;
    }
    if (resultantFormat == nullptr) {
        return STATUS_INVALID_PARAMETER;
    }

    RtlCopyMemory(resultantFormat, matchingDataRange, sizeof(KSDATAFORMAT));
    static_cast<PKSDATAFORMAT>(resultantFormat)->FormatSize =
        sizeof(KSDATAFORMAT);
    return STATUS_SUCCESS;
}

STDMETHODIMP
COpenA8DJMidiMiniport::Init(
    _In_ PUNKNOWN unknownAdapter,
    _In_ PRESOURCELIST resourceList,
    _In_ PPORTMIDI port,
    _Out_ PSERVICEGROUP *serviceGroup)
{
    UNREFERENCED_PARAMETER(unknownAdapter);
    UNREFERENCED_PARAMETER(resourceList);

    NTSTATUS status;

    if (port == nullptr || serviceGroup == nullptr || m_Port != nullptr ||
        m_ServiceGroup != nullptr) {
        return STATUS_INVALID_PARAMETER;
    }
    *serviceGroup = nullptr;
    status = PcNewServiceGroup(&m_ServiceGroup, nullptr);
    if (!NT_SUCCESS(status)) {
        return status;
    }
    m_Port = port;
    m_Port->AddRef();
    m_ServiceGroup->AddRef();
    *serviceGroup = m_ServiceGroup;
    return STATUS_SUCCESS;
}

STDMETHODIMP_(void)
COpenA8DJMidiMiniport::Service()
{
}

STDMETHODIMP
COpenA8DJMidiMiniport::NewStream(
    _Out_ PMINIPORTMIDISTREAM *stream,
    _In_opt_ PUNKNOWN outerUnknown,
    _In_ POOL_TYPE poolType,
    _In_ ULONG pin,
    _In_ BOOLEAN capture,
    _In_ PKSDATAFORMAT dataFormat,
    _Out_ PSERVICEGROUP *serviceGroup)
{
    UNREFERENCED_PARAMETER(poolType);

    if (stream == nullptr || serviceGroup == nullptr) {
        return STATUS_INVALID_PARAMETER;
    }
    *stream = nullptr;
    *serviceGroup = nullptr;

    if (outerUnknown != nullptr) {
        return STATUS_NOT_SUPPORTED;
    }
    if (m_ServiceGroup == nullptr) {
        return STATUS_DEVICE_NOT_READY;
    }
    if (!OpenA8DJMidiIsSupportedFormat(dataFormat)) {
        return STATUS_NO_MATCH;
    }
    if ((pin == OPENA8DJ_MIDI_PIN_RENDER && capture != FALSE) ||
        (pin == OPENA8DJ_MIDI_PIN_CAPTURE && capture == FALSE) ||
        pin >= OPENA8DJ_MIDI_PIN_COUNT) {
        return STATUS_INVALID_PARAMETER;
    }
    NTSTATUS status = COpenA8DJMidiStream::Create(this, capture, stream);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    /* PortCls owns this NewStream output reference independently of Init. */
    m_ServiceGroup->AddRef();
    *serviceGroup = m_ServiceGroup;
    return STATUS_SUCCESS;
}

const OPENA8DJ_MIDI_BUS_INTERFACE_V1 &
COpenA8DJMidiMiniport::BusInterface() const noexcept
{
    return m_BusInterface;
}

void
COpenA8DJMidiMiniport::NotifyPort() noexcept
{
    if (m_Port != nullptr) {
        m_Port->Notify(m_ServiceGroup);
    }
}

COpenA8DJMidiStream::COpenA8DJMidiStream(
    _In_ COpenA8DJMidiMiniport *miniport,
    _In_ BOOLEAN capture) noexcept
    : m_ReferenceCount(1),
      m_Miniport(miniport),
      m_BusStreamContext(nullptr),
      m_State(static_cast<LONG>(KSSTATE_STOP)),
      m_Capture(capture)
{
    m_Miniport->AddRef();
}

COpenA8DJMidiStream::~COpenA8DJMidiStream()
{
    if (m_BusStreamContext != nullptr) {
        m_Miniport->BusInterface().CloseStream(m_BusStreamContext);
        m_BusStreamContext = nullptr;
    }
    m_Miniport->Release();
    m_Miniport = nullptr;
}

NTSTATUS
COpenA8DJMidiStream::Create(
    _In_ COpenA8DJMidiMiniport *miniport,
    _In_ BOOLEAN capture,
    _Out_ PMINIPORTMIDISTREAM *stream)
{
    COpenA8DJMidiStream *object;
    PVOID storage;
    NTSTATUS status;

    if (miniport == nullptr || stream == nullptr) {
        return STATUS_INVALID_PARAMETER;
    }
    *stream = nullptr;
    storage = ExAllocatePool2(
        POOL_FLAG_NON_PAGED,
        sizeof(COpenA8DJMidiStream),
        OPENA8DJ_MIDI_STREAM_POOL_TAG);
    if (storage == nullptr) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    object = new (storage) COpenA8DJMidiStream(miniport, capture);
    status = object->Open();
    if (!NT_SUCCESS(status)) {
        object->Release();
        return status;
    }
    *stream = static_cast<PMINIPORTMIDISTREAM>(object);
    return STATUS_SUCCESS;
}

NTSTATUS
COpenA8DJMidiStream::Open()
{
    const OPENA8DJ_MIDI_BUS_INTERFACE_V1 &bus =
        m_Miniport->BusInterface();
    OPENA8DJ_MIDI_BUS_NOTIFY_ROUTINE notifyRoutine =
        m_Capture != FALSE ? BusNotify : nullptr;

    return bus.OpenStream(
        bus.Header.Context,
        m_Capture != FALSE ? OPENA8DJ_MIDI_BUS_DIRECTION_CAPTURE
                           : OPENA8DJ_MIDI_BUS_DIRECTION_RENDER,
        notifyRoutine,
        notifyRoutine != nullptr ? this : nullptr,
        &m_BusStreamContext);
}

VOID NTAPI
COpenA8DJMidiStream::BusNotify(_In_ PVOID notificationContext)
{
    COpenA8DJMidiStream *stream =
        static_cast<COpenA8DJMidiStream *>(notificationContext);

    if (stream != nullptr && stream->m_Capture != FALSE &&
        InterlockedCompareExchange(&stream->m_State, 0, 0) ==
            static_cast<LONG>(KSSTATE_RUN)) {
        stream->m_Miniport->NotifyPort();
    }
}

STDMETHODIMP
COpenA8DJMidiStream::QueryInterface(
    _In_ REFIID interfaceId,
    _Out_ PVOID *object)
{
    if (object == nullptr) {
        return STATUS_INVALID_PARAMETER;
    }
    *object = nullptr;
    if (IsEqualGUIDAligned(interfaceId, IID_IUnknown) ||
        IsEqualGUIDAligned(interfaceId, IID_IMiniportMidiStream)) {
        *object = static_cast<PMINIPORTMIDISTREAM>(this);
        AddRef();
        return STATUS_SUCCESS;
    }
    return STATUS_NOINTERFACE;
}

STDMETHODIMP_(ULONG)
COpenA8DJMidiStream::AddRef()
{
    return static_cast<ULONG>(InterlockedIncrement(&m_ReferenceCount));
}

STDMETHODIMP_(ULONG)
COpenA8DJMidiStream::Release()
{
    ULONG referenceCount =
        static_cast<ULONG>(InterlockedDecrement(&m_ReferenceCount));

    if (referenceCount == 0u) {
        this->~COpenA8DJMidiStream();
        ExFreePoolWithTag(this, OPENA8DJ_MIDI_STREAM_POOL_TAG);
    }
    return referenceCount;
}

STDMETHODIMP
COpenA8DJMidiStream::SetFormat(_In_ PKSDATAFORMAT dataFormat)
{
    if (InterlockedCompareExchange(&m_State, 0, 0) !=
        static_cast<LONG>(KSSTATE_STOP)) {
        return STATUS_INVALID_DEVICE_STATE;
    }
    return OpenA8DJMidiIsSupportedFormat(dataFormat)
               ? STATUS_SUCCESS
               : STATUS_NO_MATCH;
}

STDMETHODIMP
COpenA8DJMidiStream::SetState(_In_ KSSTATE state)
{
    NTSTATUS status;

    if (state < KSSTATE_STOP || state > KSSTATE_RUN) {
        return STATUS_INVALID_PARAMETER;
    }
    if (m_BusStreamContext == nullptr) {
        return STATUS_DEVICE_NOT_READY;
    }
    if (static_cast<LONG>(state) ==
        InterlockedCompareExchange(&m_State, 0, 0)) {
        return STATUS_SUCCESS;
    }

    status = m_Miniport->BusInterface().SetState(
        m_BusStreamContext,
        static_cast<ULONG>(state));
    if (NT_SUCCESS(status)) {
        /* Publish RUN before forcing the notification that closes the edge. */
        InterlockedExchange(&m_State, static_cast<LONG>(state));
        if (m_Capture != FALSE && state == KSSTATE_RUN) {
            m_Miniport->NotifyPort();
        }
    }
    return status;
}

STDMETHODIMP
COpenA8DJMidiStream::Read(
    _In_ PVOID bufferAddress,
    _In_ ULONG bufferLength,
    _Out_ PULONG bytesRead)
{
    NTSTATUS status;

    if (bytesRead == nullptr) {
        return STATUS_INVALID_PARAMETER;
    }
    *bytesRead = 0u;
    if (m_Capture == FALSE) {
        return STATUS_INVALID_DEVICE_REQUEST;
    }
    if (bufferAddress == nullptr && bufferLength != 0u) {
        return STATUS_INVALID_PARAMETER;
    }
    if (InterlockedCompareExchange(&m_State, 0, 0) !=
            static_cast<LONG>(KSSTATE_RUN) ||
        m_BusStreamContext == nullptr) {
        return STATUS_INVALID_DEVICE_STATE;
    }
    if (bufferLength == 0u) {
        return STATUS_SUCCESS;
    }

    status = m_Miniport->BusInterface().ReadStream(
        m_BusStreamContext,
        bufferAddress,
        bufferLength,
        bytesRead);
    if (NT_SUCCESS(status) && *bytesRead > bufferLength) {
        *bytesRead = 0u;
        return STATUS_DEVICE_PROTOCOL_ERROR;
    }
    return status;
}

STDMETHODIMP
COpenA8DJMidiStream::Write(
    _In_ PVOID bufferAddress,
    _In_ ULONG bytesToWrite,
    _Out_ PULONG bytesWritten)
{
    OPENA8DJ_MIDI_BUS_WRITE_STATUS_CLASS statusClass;
    OPENA8DJ_MIDI_PORTCLS_WRITE_ACTION action;
    NTSTATUS status;

    if (bytesWritten == nullptr) {
        return STATUS_INVALID_PARAMETER;
    }
    *bytesWritten = 0u;
    if (m_Capture != FALSE) {
        return STATUS_INVALID_DEVICE_REQUEST;
    }
    if (bufferAddress == nullptr && bytesToWrite != 0u) {
        return STATUS_INVALID_PARAMETER;
    }
    if (InterlockedCompareExchange(&m_State, 0, 0) !=
            static_cast<LONG>(KSSTATE_RUN) ||
        m_BusStreamContext == nullptr) {
        return STATUS_INVALID_DEVICE_STATE;
    }
    if (bytesToWrite == 0u) {
        return STATUS_SUCCESS;
    }

    status = m_Miniport->BusInterface().WriteStream(
        m_BusStreamContext,
        bufferAddress,
        bytesToWrite,
        bytesWritten);

    statusClass = NT_SUCCESS(status)
                      ? OPENA8DJ_MIDI_BUS_WRITE_STATUS_SUCCESS
                      : (status == STATUS_DEVICE_BUSY
                             ? OPENA8DJ_MIDI_BUS_WRITE_STATUS_BUSY
                             : OPENA8DJ_MIDI_BUS_WRITE_STATUS_FAILURE);
    action = OpenA8DJMidiClassifyBusWriteResult(
        statusClass,
        bytesToWrite,
        *bytesWritten);
    if (action == OPENA8DJ_MIDI_PORTCLS_WRITE_COMPLETE) {
        return STATUS_SUCCESS;
    }

    *bytesWritten = 0u;
    if (action == OPENA8DJ_MIDI_PORTCLS_WRITE_BACKPRESSURE) {
        /* PortCls defines success with zero bytes as retry-later backpressure. */
        return STATUS_SUCCESS;
    }
    if (action == OPENA8DJ_MIDI_PORTCLS_WRITE_PROTOCOL_ERROR) {
        return STATUS_DEVICE_PROTOCOL_ERROR;
    }
    return status;
}

NTSTATUS
OpenA8DJMidiCreateMiniport(
    _In_ const OPENA8DJ_MIDI_BUS_INTERFACE_V1 *busInterface,
    _Out_ PMINIPORTMIDI *miniport)
{
    COpenA8DJMidiMiniport *object;
    PVOID storage;

    if (busInterface == nullptr || miniport == nullptr) {
        return STATUS_INVALID_PARAMETER;
    }
    *miniport = nullptr;
    storage = ExAllocatePool2(
        POOL_FLAG_NON_PAGED,
        sizeof(COpenA8DJMidiMiniport),
        OPENA8DJ_MIDI_MINIPORT_POOL_TAG);
    if (storage == nullptr) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    object = new (storage) COpenA8DJMidiMiniport(busInterface);
    /* The object now owns exactly the caller's one queried-interface reference. */
    *miniport = static_cast<PMINIPORTMIDI>(object);
    return STATUS_SUCCESS;
}
