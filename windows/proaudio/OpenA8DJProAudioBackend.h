#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace opena8dj::proaudio {

enum class EndpointDirection {
    Render,
    Capture
};

struct EndpointDescriptor {
    EndpointDirection direction = EndpointDirection::Render;
    bool active = false;
    bool containerVerifiedOpenA8DJUsb = false;
    std::uint32_t mixChannels = 0;
    std::wstring endpointId;
    std::wstring friendlyName;
    std::wstring interfaceFriendlyName;
    std::wstring instanceId;
    std::wstring containerId;
    std::wstring driverProvider;
};

enum class EndpointSelectionStatus {
    Selected,
    MissingRender,
    MissingCapture,
    AmbiguousRender,
    AmbiguousCapture,
    UnverifiedIdentity,
    NoCommonIdentity
};

struct EndpointSelectionResult {
    EndpointSelectionStatus status = EndpointSelectionStatus::MissingRender;
    std::size_t renderIndex = static_cast<std::size_t>(-1);
    std::size_t captureIndex = static_cast<std::size_t>(-1);
    std::wstring commonIdentity;
    std::wstring detail;
};

bool IsOpenA8DJAggregateEndpoint(const EndpointDescriptor& endpoint);

EndpointSelectionResult SelectOpenA8DJAggregatePair(
    const std::vector<EndpointDescriptor>& endpoints);

const wchar_t* EndpointSelectionStatusName(EndpointSelectionStatus status);

}  // namespace opena8dj::proaudio
