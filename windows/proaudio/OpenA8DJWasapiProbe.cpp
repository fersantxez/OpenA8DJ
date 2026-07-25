#include <windows.h>

#include <initguid.h>
#include <audioclient.h>
#include <devpkey.h>
#include <propkeydef.h>
#include <functiondiscoverykeys_devpkey.h>
#include <ksmedia.h>
#include <mmdeviceapi.h>
#include <propvarutil.h>
#include <setupapi.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cwctype>
#include <iostream>
#include <iterator>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "OpenA8DJProAudioBackend.h"

namespace {

using opena8dj::proaudio::EndpointDescriptor;
using opena8dj::proaudio::EndpointDirection;

template <typename T>
class ComPtr {
public:
    ComPtr() = default;
    ~ComPtr() { Reset(); }
    ComPtr(const ComPtr&) = delete;
    ComPtr& operator=(const ComPtr&) = delete;

    T* Get() const { return value_; }
    T** Put()
    {
        Reset();
        return &value_;
    }
    T* operator->() const { return value_; }
    explicit operator bool() const { return value_ != nullptr; }
    void Reset()
    {
        if (value_ != nullptr) {
            value_->Release();
            value_ = nullptr;
        }
    }

private:
    T* value_ = nullptr;
};

struct FormatProbe {
    std::uint32_t sampleRate = 0;
    HRESULT status = E_FAIL;
};

struct EndpointProbe {
    EndpointDescriptor descriptor;
    HRESULT propertyStatus = E_FAIL;
    HRESULT activateStatus = E_FAIL;
    HRESULT periodStatus = E_FAIL;
    HRESULT mixFormatStatus = E_FAIL;
    REFERENCE_TIME defaultPeriod = 0;
    REFERENCE_TIME minimumPeriod = 0;
    WORD mixBitsPerSample = 0;
    DWORD mixSampleRate = 0;
    std::array<FormatProbe, 2> formats{{{44100, E_FAIL}, {48000, E_FAIL}}};
};

struct UsbDevnodeProbe {
    std::wstring instanceId;
    std::wstring containerId;
    std::wstring service;
    std::wstring driverProvider;
    bool verifiedOpenA8DJUsb = false;
};

class DeviceInfoSet {
public:
    explicit DeviceInfoSet(HDEVINFO value) : value_(value) {}
    ~DeviceInfoSet()
    {
        if (value_ != INVALID_HANDLE_VALUE) {
            SetupDiDestroyDeviceInfoList(value_);
        }
    }
    DeviceInfoSet(const DeviceInfoSet&) = delete;
    DeviceInfoSet& operator=(const DeviceInfoSet&) = delete;
    HDEVINFO Get() const { return value_; }

private:
    HDEVINFO value_ = INVALID_HANDLE_VALUE;
};

std::wstring Normalize(std::wstring value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](wchar_t ch) {
        return static_cast<wchar_t>(std::towlower(ch));
    });
    return value;
}

bool StartsWith(const std::wstring& value, const wchar_t* prefix)
{
    return Normalize(value).rfind(prefix, 0) == 0;
}

bool Equals(const std::wstring& value, const wchar_t* expected)
{
    return Normalize(value) == expected;
}

bool IsAudio8UsbInstanceId(const std::wstring& instanceId)
{
    static constexpr wchar_t identity[] = L"usb\\vid_17cc&pid_1978";
    const std::wstring normalized = Normalize(instanceId);
    if (normalized.rfind(identity, 0) != 0) {
        return false;
    }
    const std::size_t identityLength = std::size(identity) - 1;
    return normalized.size() == identityLength ||
           normalized[identityLength] == L'\\' ||
           normalized[identityLength] == L'&';
}

std::wstring PropertyToString(IPropertyStore* store, REFPROPERTYKEY key)
{
    if (store == nullptr) {
        return {};
    }
    PROPVARIANT value;
    PropVariantInit(&value);
    const HRESULT status = store->GetValue(key, &value);
    if (FAILED(status)) {
        PropVariantClear(&value);
        return {};
    }

    std::wstring result;
    if (value.vt == VT_LPWSTR && value.pwszVal != nullptr) {
        result = value.pwszVal;
    } else if (value.vt == VT_BSTR && value.bstrVal != nullptr) {
        result.assign(value.bstrVal, SysStringLen(value.bstrVal));
    } else if (value.vt == VT_CLSID && value.puuid != nullptr) {
        wchar_t text[64] = {};
        if (StringFromGUID2(*value.puuid, text, static_cast<int>(std::size(text))) > 0) {
            result = text;
        }
    }
    PropVariantClear(&value);
    return result;
}

std::wstring JsonEscape(const std::wstring& value)
{
    std::wostringstream escaped;
    for (const wchar_t ch : value) {
        switch (ch) {
        case L'\\': escaped << L"\\\\"; break;
        case L'\"': escaped << L"\\\""; break;
        case L'\b': escaped << L"\\b"; break;
        case L'\f': escaped << L"\\f"; break;
        case L'\n': escaped << L"\\n"; break;
        case L'\r': escaped << L"\\r"; break;
        case L'\t': escaped << L"\\t"; break;
        default:
            if (static_cast<unsigned int>(ch) < 0x20u) {
                escaped << L"\\u" << std::hex << std::uppercase;
                escaped.width(4);
                escaped.fill(L'0');
                escaped << static_cast<unsigned int>(ch) << std::dec;
            } else {
                escaped << ch;
            }
            break;
        }
    }
    return escaped.str();
}

std::wstring HResultHex(HRESULT status)
{
    std::wostringstream stream;
    stream << L"0x" << std::hex << std::uppercase;
    stream.width(8);
    stream.fill(L'0');
    stream << static_cast<unsigned long>(status);
    return stream.str();
}

std::wstring GuidToString(const GUID& value)
{
    wchar_t text[64] = {};
    if (StringFromGUID2(value, text, static_cast<int>(std::size(text))) <= 0) {
        return {};
    }
    return text;
}

std::wstring GetDevnodeStringProperty(
    HDEVINFO devices,
    SP_DEVINFO_DATA* device,
    const DEVPROPKEY& key)
{
    DEVPROPTYPE propertyType = 0;
    DWORD requiredBytes = 0;
    if (!SetupDiGetDevicePropertyW(
            devices, device, &key, &propertyType, nullptr, 0, &requiredBytes, 0) &&
        GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
        return {};
    }
    if (requiredBytes < sizeof(wchar_t)) {
        return {};
    }
    std::vector<BYTE> buffer(requiredBytes, 0);
    if (!SetupDiGetDevicePropertyW(
            devices,
            device,
            &key,
            &propertyType,
            buffer.data(),
            static_cast<DWORD>(buffer.size()),
            nullptr,
            0) ||
        propertyType != DEVPROP_TYPE_STRING) {
        return {};
    }
    return reinterpret_cast<const wchar_t*>(buffer.data());
}

std::wstring GetDevnodeInstanceId(HDEVINFO devices, SP_DEVINFO_DATA* device)
{
    DWORD requiredCharacters = 0;
    if (!SetupDiGetDeviceInstanceIdW(devices, device, nullptr, 0, &requiredCharacters) &&
        GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
        return {};
    }
    if (requiredCharacters == 0) {
        return {};
    }
    std::vector<wchar_t> buffer(requiredCharacters, L'\0');
    if (!SetupDiGetDeviceInstanceIdW(
            devices,
            device,
            buffer.data(),
            static_cast<DWORD>(buffer.size()),
            nullptr)) {
        return {};
    }
    return buffer.data();
}

std::wstring GetDevnodeContainerId(HDEVINFO devices, SP_DEVINFO_DATA* device)
{
    DEVPROPTYPE propertyType = 0;
    GUID container{};
    DWORD requiredBytes = 0;
    if (!SetupDiGetDevicePropertyW(
            devices,
            device,
            &DEVPKEY_Device_ContainerId,
            &propertyType,
            reinterpret_cast<PBYTE>(&container),
            static_cast<DWORD>(sizeof(container)),
            &requiredBytes,
            0) ||
        propertyType != DEVPROP_TYPE_GUID ||
        requiredBytes != static_cast<DWORD>(sizeof(container))) {
        return {};
    }
    return GuidToString(container);
}

HRESULT EnumerateAudio8UsbDevnodes(std::vector<UsbDevnodeProbe>* matches)
{
    const HDEVINFO rawDevices = SetupDiGetClassDevsW(
        nullptr, nullptr, nullptr, DIGCF_ALLCLASSES | DIGCF_PRESENT);
    if (rawDevices == INVALID_HANDLE_VALUE) {
        return HRESULT_FROM_WIN32(GetLastError());
    }
    DeviceInfoSet devices(rawDevices);

    for (DWORD index = 0;; ++index) {
        SP_DEVINFO_DATA device{};
        device.cbSize = sizeof(device);
        if (!SetupDiEnumDeviceInfo(devices.Get(), index, &device)) {
            const DWORD error = GetLastError();
            return error == ERROR_NO_MORE_ITEMS ? S_OK : HRESULT_FROM_WIN32(error);
        }

        const std::wstring instanceId = GetDevnodeInstanceId(devices.Get(), &device);
        if (!IsAudio8UsbInstanceId(instanceId)) {
            continue;
        }

        UsbDevnodeProbe match;
        match.instanceId = instanceId;
        match.containerId = GetDevnodeContainerId(devices.Get(), &device);
        match.service = GetDevnodeStringProperty(
            devices.Get(), &device, DEVPKEY_Device_Service);
        match.driverProvider = GetDevnodeStringProperty(
            devices.Get(), &device, DEVPKEY_Device_DriverProvider);
        match.verifiedOpenA8DJUsb =
            !match.containerId.empty() &&
            (Equals(match.service, L"opena8djusb") ||
             Equals(match.service, L"opena8djusbacx")) &&
            Equals(match.driverProvider, L"opena8dj");
        matches->push_back(std::move(match));
    }
}

bool ContainerWasVerified(
    const std::wstring& containerId,
    const std::vector<UsbDevnodeProbe>& devnodes)
{
    if (containerId.empty()) {
        return false;
    }
    const std::wstring normalized = Normalize(containerId);
    for (const UsbDevnodeProbe& devnode : devnodes) {
        if (devnode.verifiedOpenA8DJUsb &&
            Normalize(devnode.containerId) == normalized) {
            return true;
        }
    }
    return false;
}

WAVEFORMATEXTENSIBLE MakePcm16Format(DWORD sampleRate)
{
    WAVEFORMATEXTENSIBLE format{};
    format.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
    format.Format.nChannels = 8;
    format.Format.nSamplesPerSec = sampleRate;
    format.Format.wBitsPerSample = 16;
    format.Format.nBlockAlign = static_cast<WORD>(format.Format.nChannels * sizeof(std::int16_t));
    format.Format.nAvgBytesPerSec = format.Format.nSamplesPerSec * format.Format.nBlockAlign;
    format.Format.cbSize = static_cast<WORD>(
        sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX));
    format.Samples.wValidBitsPerSample = 16;
    format.dwChannelMask = KSAUDIO_SPEAKER_7POINT1_SURROUND;
    format.SubFormat = KSDATAFORMAT_SUBTYPE_PCM;
    return format;
}

EndpointProbe InspectEndpoint(IMMDevice* device, EndpointDirection direction)
{
    EndpointProbe probe;
    probe.descriptor.direction = direction;
    probe.descriptor.active = true;

    LPWSTR endpointId = nullptr;
    if (SUCCEEDED(device->GetId(&endpointId)) && endpointId != nullptr) {
        probe.descriptor.endpointId = endpointId;
    }
    CoTaskMemFree(endpointId);

    ComPtr<IPropertyStore> properties;
    probe.propertyStatus = device->OpenPropertyStore(STGM_READ, properties.Put());
    if (SUCCEEDED(probe.propertyStatus)) {
        probe.descriptor.friendlyName = PropertyToString(properties.Get(), PKEY_Device_FriendlyName);
        probe.descriptor.interfaceFriendlyName =
            PropertyToString(properties.Get(), PKEY_DeviceInterface_FriendlyName);
        probe.descriptor.instanceId = PropertyToString(properties.Get(), PKEY_Device_InstanceId);
        probe.descriptor.containerId = PropertyToString(properties.Get(), PKEY_Device_ContainerId);
        probe.descriptor.driverProvider = PropertyToString(properties.Get(), PKEY_Device_DriverProvider);
    }

    ComPtr<IAudioClient> client;
    probe.activateStatus = device->Activate(
        __uuidof(IAudioClient),
        CLSCTX_INPROC_SERVER,
        nullptr,
        reinterpret_cast<void**>(client.Put()));
    if (FAILED(probe.activateStatus)) {
        return probe;
    }

    probe.periodStatus = client->GetDevicePeriod(&probe.defaultPeriod, &probe.minimumPeriod);

    WAVEFORMATEX* mixFormat = nullptr;
    probe.mixFormatStatus = client->GetMixFormat(&mixFormat);
    if (SUCCEEDED(probe.mixFormatStatus) && mixFormat != nullptr) {
        probe.descriptor.mixChannels = mixFormat->nChannels;
        probe.mixBitsPerSample = mixFormat->wBitsPerSample;
        probe.mixSampleRate = mixFormat->nSamplesPerSec;
    }
    CoTaskMemFree(mixFormat);

    for (FormatProbe& formatProbe : probe.formats) {
        WAVEFORMATEXTENSIBLE format = MakePcm16Format(formatProbe.sampleRate);
        formatProbe.status = client->IsFormatSupported(
            AUDCLNT_SHAREMODE_EXCLUSIVE,
            &format.Format,
            nullptr);
    }
    return probe;
}

HRESULT EnumerateDirection(
    IMMDeviceEnumerator* enumerator,
    EDataFlow flow,
    EndpointDirection direction,
    std::vector<EndpointProbe>* probes)
{
    ComPtr<IMMDeviceCollection> collection;
    HRESULT status = enumerator->EnumAudioEndpoints(flow, DEVICE_STATE_ACTIVE, collection.Put());
    if (FAILED(status)) {
        return status;
    }
    UINT count = 0;
    status = collection->GetCount(&count);
    if (FAILED(status)) {
        return status;
    }
    for (UINT index = 0; index < count; ++index) {
        ComPtr<IMMDevice> device;
        status = collection->Item(index, device.Put());
        if (FAILED(status)) {
            return status;
        }
        probes->push_back(InspectEndpoint(device.Get(), direction));
    }
    return S_OK;
}

bool FormatsPassed(const EndpointProbe& probe)
{
    return probe.formats[0].status == S_OK && probe.formats[1].status == S_OK;
}

void EmitEndpointJson(const EndpointProbe& probe, std::size_t index)
{
    const EndpointDescriptor& endpoint = probe.descriptor;
    std::wcout
        << L"    {\"index\":" << index
        << L",\"direction\":\""
        << (endpoint.direction == EndpointDirection::Render ? L"render" : L"capture")
        << L"\",\"active\":" << (endpoint.active ? L"true" : L"false")
        << L",\"container_verified_opena8djusb\":"
        << (endpoint.containerVerifiedOpenA8DJUsb ? L"true" : L"false")
        << L",\"endpoint_id\":\"" << JsonEscape(endpoint.endpointId)
        << L"\",\"friendly_name\":\"" << JsonEscape(endpoint.friendlyName)
        << L"\",\"interface_friendly_name\":\"" << JsonEscape(endpoint.interfaceFriendlyName)
        << L"\",\"instance_id\":\"" << JsonEscape(endpoint.instanceId)
        << L"\",\"container_id\":\"" << JsonEscape(endpoint.containerId)
        << L"\",\"driver_provider\":\"" << JsonEscape(endpoint.driverProvider)
        << L"\",\"mix_channels\":" << endpoint.mixChannels
        << L",\"mix_bits_per_sample\":" << probe.mixBitsPerSample
        << L",\"mix_sample_rate\":" << probe.mixSampleRate
        << L",\"default_period_100ns\":" << probe.defaultPeriod
        << L",\"minimum_period_100ns\":" << probe.minimumPeriod
        << L",\"property_status\":\"" << HResultHex(probe.propertyStatus)
        << L"\",\"activate_status\":\"" << HResultHex(probe.activateStatus)
        << L"\",\"period_status\":\"" << HResultHex(probe.periodStatus)
        << L"\",\"mix_format_status\":\"" << HResultHex(probe.mixFormatStatus)
        << L"\",\"exclusive_pcm16_8ch_44100\":\"" << HResultHex(probe.formats[0].status)
        << L"\",\"exclusive_pcm16_8ch_48000\":\"" << HResultHex(probe.formats[1].status)
        << L"\"}";
}

void EmitUsbDevnodeJson(const UsbDevnodeProbe& devnode, std::size_t index)
{
    std::wcout
        << L"    {\"index\":" << index
        << L",\"instance_id\":\"" << JsonEscape(devnode.instanceId)
        << L"\",\"container_id\":\"" << JsonEscape(devnode.containerId)
        << L"\",\"service\":\"" << JsonEscape(devnode.service)
        << L"\",\"driver_provider\":\"" << JsonEscape(devnode.driverProvider)
        << L"\",\"verified_opena8djusb\":"
        << (devnode.verifiedOpenA8DJUsb ? L"true" : L"false")
        << L"}";
}

}  // namespace

int wmain()
{
    const HRESULT initializeStatus = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(initializeStatus)) {
        std::wcout << L"{\"schema_version\":1,\"pass\":false,\"error\":\"CoInitializeEx failed: "
                   << HResultHex(initializeStatus) << L"\"}\n";
        return 2;
    }

    int exitCode = 0;
    {
        ComPtr<IMMDeviceEnumerator> enumerator;
        const HRESULT createStatus = CoCreateInstance(
            __uuidof(MMDeviceEnumerator),
            nullptr,
            CLSCTX_INPROC_SERVER,
            __uuidof(IMMDeviceEnumerator),
            reinterpret_cast<void**>(enumerator.Put()));
        if (FAILED(createStatus)) {
            std::wcout << L"{\"schema_version\":1,\"pass\":false,\"error\":\"MMDeviceEnumerator failed: "
                       << HResultHex(createStatus) << L"\"}\n";
            CoUninitialize();
            return 2;
        }

        std::vector<EndpointProbe> probes;
        std::vector<UsbDevnodeProbe> usbDevnodes;
        const HRESULT pnpStatus = EnumerateAudio8UsbDevnodes(&usbDevnodes);
        const HRESULT renderStatus =
            EnumerateDirection(enumerator.Get(), eRender, EndpointDirection::Render, &probes);
        const HRESULT captureStatus =
            EnumerateDirection(enumerator.Get(), eCapture, EndpointDirection::Capture, &probes);

        for (EndpointProbe& probe : probes) {
            probe.descriptor.containerVerifiedOpenA8DJUsb =
                SUCCEEDED(pnpStatus) &&
                ContainerWasVerified(probe.descriptor.containerId, usbDevnodes);
        }

        std::vector<EndpointDescriptor> descriptors;
        descriptors.reserve(probes.size());
        for (const EndpointProbe& probe : probes) {
            descriptors.push_back(probe.descriptor);
        }
        const auto selection = opena8dj::proaudio::SelectOpenA8DJAggregatePair(descriptors);

        bool selectedFormatsPass = false;
        bool selectedPeriodsPass = false;
        if (selection.status == opena8dj::proaudio::EndpointSelectionStatus::Selected) {
            selectedFormatsPass = FormatsPassed(probes[selection.renderIndex]) &&
                                  FormatsPassed(probes[selection.captureIndex]);
            selectedPeriodsPass =
                probes[selection.renderIndex].periodStatus == S_OK &&
                probes[selection.captureIndex].periodStatus == S_OK;
        }
        const bool pass = SUCCEEDED(pnpStatus) &&
                          SUCCEEDED(renderStatus) && SUCCEEDED(captureStatus) &&
                          selection.status == opena8dj::proaudio::EndpointSelectionStatus::Selected &&
                          selectedFormatsPass && selectedPeriodsPass;
        exitCode = pass ? 0 : 1;

        std::wcout
            << L"{\n  \"schema_version\":1,\n"
            << L"  \"safety_policy\":\"enumerate_query_only_no_initialize_no_start\",\n"
            << L"  \"pass\":" << (pass ? L"true" : L"false") << L",\n"
            << L"  \"pnp_verification_status\":\"" << HResultHex(pnpStatus) << L"\",\n"
            << L"  \"render_enumeration_status\":\"" << HResultHex(renderStatus) << L"\",\n"
            << L"  \"capture_enumeration_status\":\"" << HResultHex(captureStatus) << L"\",\n"
            << L"  \"selection_status\":\""
            << opena8dj::proaudio::EndpointSelectionStatusName(selection.status) << L"\",\n"
            << L"  \"selection_detail\":\"" << JsonEscape(selection.detail) << L"\",\n"
            << L"  \"common_identity\":\"" << JsonEscape(selection.commonIdentity) << L"\",\n"
            << L"  \"selected_render_index\":";
        if (selection.status == opena8dj::proaudio::EndpointSelectionStatus::Selected) {
            std::wcout << selection.renderIndex;
        } else {
            std::wcout << L"null";
        }
        std::wcout << L",\n  \"selected_capture_index\":";
        if (selection.status == opena8dj::proaudio::EndpointSelectionStatus::Selected) {
            std::wcout << selection.captureIndex;
        } else {
            std::wcout << L"null";
        }
        std::wcout << L",\n  \"selected_formats_pass\":"
                   << (selectedFormatsPass ? L"true" : L"false")
                   << L",\n  \"selected_periods_pass\":"
                   << (selectedPeriodsPass ? L"true" : L"false")
                   << L",\n  \"usb_devnode_count\":" << usbDevnodes.size()
                   << L",\n  \"usb_devnodes\":[\n";
        for (std::size_t index = 0; index < usbDevnodes.size(); ++index) {
            EmitUsbDevnodeJson(usbDevnodes[index], index);
            std::wcout << (index + 1 == usbDevnodes.size() ? L"\n" : L",\n");
        }
        std::wcout << L"  ],\n  \"endpoint_count\":" << probes.size()
                   << L",\n  \"endpoints\":[\n";
        for (std::size_t index = 0; index < probes.size(); ++index) {
            EmitEndpointJson(probes[index], index);
            std::wcout << (index + 1 == probes.size() ? L"\n" : L",\n");
        }
        std::wcout << L"  ]\n}\n";
    }
    CoUninitialize();
    return exitCode;
}
