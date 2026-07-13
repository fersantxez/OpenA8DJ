#include "OpenA8DJProAudioBackend.h"

#include <algorithm>
#include <cwctype>
#include <utility>

namespace opena8dj::proaudio {
namespace {

std::wstring Normalize(std::wstring value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](wchar_t ch) {
        return static_cast<wchar_t>(std::towlower(ch));
    });
    return value;
}

bool Contains(const std::wstring& value, const wchar_t* needle)
{
    return Normalize(value).find(needle) != std::wstring::npos;
}

bool HasAggregateRoleName(const EndpointDescriptor& endpoint)
{
    const std::wstring friendly = Normalize(endpoint.friendlyName);
    const std::wstring interfaceFriendly = Normalize(endpoint.interfaceFriendlyName);
    if (endpoint.direction == EndpointDirection::Render) {
        return friendly.find(L"audio 8 dj (8ch out)") != std::wstring::npos ||
               interfaceFriendly.find(L"audio 8 dj (8ch out)") != std::wstring::npos ||
               Contains(interfaceFriendly, L"opena8dj render a");
    }
    return friendly.find(L"audio 8 dj (8ch in)") != std::wstring::npos ||
           interfaceFriendly.find(L"audio 8 dj (8ch in)") != std::wstring::npos ||
           Contains(interfaceFriendly, L"opena8dj capture a");
}

std::wstring CommonIdentity(
    const EndpointDescriptor& render,
    const EndpointDescriptor& capture)
{
    // ContainerId is the authoritative cross-endpoint identity.  Never fall
    // back to IMMDevice endpoint IDs (opaque) or instance IDs when two
    // present ContainerIds disagree.
    if (!render.containerId.empty() && !capture.containerId.empty() &&
        Normalize(render.containerId) == Normalize(capture.containerId)) {
        return render.containerId;
    }
    return {};
}

}  // namespace

bool IsOpenA8DJAggregateEndpoint(const EndpointDescriptor& endpoint)
{
    // The shared-mode mix format is diagnostic only.  Exclusive 8-channel
    // support is validated separately with IAudioClient::IsFormatSupported.
    return endpoint.active && HasAggregateRoleName(endpoint);
}

EndpointSelectionResult SelectOpenA8DJAggregatePair(
    const std::vector<EndpointDescriptor>& endpoints)
{
    std::vector<std::size_t> renderRoleCandidates;
    std::vector<std::size_t> captureRoleCandidates;

    for (std::size_t index = 0; index < endpoints.size(); ++index) {
        if (!IsOpenA8DJAggregateEndpoint(endpoints[index])) {
            continue;
        }
        if (endpoints[index].direction == EndpointDirection::Render) {
            renderRoleCandidates.push_back(index);
        } else {
            captureRoleCandidates.push_back(index);
        }
    }

    EndpointSelectionResult result;
    if (renderRoleCandidates.empty()) {
        result.status = EndpointSelectionStatus::MissingRender;
        result.detail = L"No active OpenA8DJ Render A role endpoint was found.";
        return result;
    }
    if (captureRoleCandidates.empty()) {
        result.status = EndpointSelectionStatus::MissingCapture;
        result.detail = L"No active OpenA8DJ Capture A role endpoint was found.";
        return result;
    }

    std::vector<std::size_t> renderCandidates;
    std::vector<std::size_t> captureCandidates;
    for (const std::size_t index : renderRoleCandidates) {
        if (endpoints[index].containerVerifiedOpenA8DJUsb &&
            !endpoints[index].containerId.empty()) {
            renderCandidates.push_back(index);
        }
    }
    for (const std::size_t index : captureRoleCandidates) {
        if (endpoints[index].containerVerifiedOpenA8DJUsb &&
            !endpoints[index].containerId.empty()) {
            captureCandidates.push_back(index);
        }
    }
    if (renderCandidates.empty() || captureCandidates.empty()) {
        result.status = EndpointSelectionStatus::UnverifiedIdentity;
        result.detail = L"Role endpoints exist, but their ContainerId was not verified against a present OpenA8DJ USB devnode.";
        return result;
    }

    std::vector<EndpointSelectionResult> matchingPairs;
    for (const std::size_t renderIndex : renderCandidates) {
        for (const std::size_t captureIndex : captureCandidates) {
            const std::wstring identity =
                CommonIdentity(endpoints[renderIndex], endpoints[captureIndex]);
            if (!identity.empty()) {
                EndpointSelectionResult match;
                match.status = EndpointSelectionStatus::Selected;
                match.renderIndex = renderIndex;
                match.captureIndex = captureIndex;
                match.commonIdentity = identity;
                match.detail = L"Selected the unique aggregate render/capture pair with common identity.";
                matchingPairs.push_back(std::move(match));
            }
        }
    }

    if (matchingPairs.size() == 1) {
        return matchingPairs.front();
    }
    if (matchingPairs.empty()) {
        result.status = EndpointSelectionStatus::NoCommonIdentity;
        result.detail = L"Aggregate endpoints were found, but no render/capture pair shared a container or instance identity.";
        return result;
    }

    // Report which side is ambiguous so diagnostics remain useful when Windows
    // retains stale endpoint instances after a driver upgrade.
    if (renderCandidates.size() > 1) {
        result.status = EndpointSelectionStatus::AmbiguousRender;
        result.detail = L"More than one aggregate Render A endpoint matched a Capture A endpoint.";
    } else {
        result.status = EndpointSelectionStatus::AmbiguousCapture;
        result.detail = L"More than one aggregate Capture A endpoint matched a Render A endpoint.";
    }
    return result;
}

const wchar_t* EndpointSelectionStatusName(EndpointSelectionStatus status)
{
    switch (status) {
    case EndpointSelectionStatus::Selected:
        return L"selected";
    case EndpointSelectionStatus::MissingRender:
        return L"missing-render";
    case EndpointSelectionStatus::MissingCapture:
        return L"missing-capture";
    case EndpointSelectionStatus::AmbiguousRender:
        return L"ambiguous-render";
    case EndpointSelectionStatus::AmbiguousCapture:
        return L"ambiguous-capture";
    case EndpointSelectionStatus::UnverifiedIdentity:
        return L"unverified-identity";
    case EndpointSelectionStatus::NoCommonIdentity:
        return L"no-common-identity";
    }
    return L"unknown";
}

}  // namespace opena8dj::proaudio
