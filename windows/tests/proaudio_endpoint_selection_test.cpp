#include "../proaudio/OpenA8DJProAudioBackend.h"

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

using opena8dj::proaudio::EndpointDescriptor;
using opena8dj::proaudio::EndpointDirection;
using opena8dj::proaudio::EndpointSelectionStatus;

namespace {

int failures = 0;

void Expect(bool condition, const char* message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

EndpointDescriptor Endpoint(
    EndpointDirection direction,
    const wchar_t* name,
    const wchar_t* container,
    std::uint32_t channels = 8,
    bool active = true)
{
    EndpointDescriptor endpoint;
    endpoint.direction = direction;
    endpoint.active = active;
    endpoint.containerVerifiedOpenA8DJUsb = true;
    endpoint.mixChannels = channels;
    endpoint.friendlyName = name;
    endpoint.containerId = container;
    return endpoint;
}

void TestUniquePair()
{
    std::vector<EndpointDescriptor> endpoints{
        Endpoint(EndpointDirection::Render, L"Speakers", L"{OTHER}", 2),
        Endpoint(EndpointDirection::Capture, L"Audio 8 DJ (8ch In)", L"{A8DJ}"),
        Endpoint(EndpointDirection::Render, L"Audio 8 DJ (8ch Out)", L"{a8dj}"),
        Endpoint(EndpointDirection::Render, L"Audio 8 DJ (Ch B, Out 3|4)", L"{A8DJ}", 2),
    };
    const auto result = opena8dj::proaudio::SelectOpenA8DJAggregatePair(endpoints);
    Expect(result.status == EndpointSelectionStatus::Selected, "unique aggregate pair should be selected");
    Expect(result.renderIndex == 2, "selected render index should identify Render A");
    Expect(result.captureIndex == 1, "selected capture index should identify Capture A");
    Expect(!result.commonIdentity.empty(), "selected pair should publish its common identity");
}

void TestIdentityMismatch()
{
    std::vector<EndpointDescriptor> endpoints{
        Endpoint(EndpointDirection::Render, L"Audio 8 DJ (8ch Out)", L"{ONE}"),
        Endpoint(EndpointDirection::Capture, L"Audio 8 DJ (8ch In)", L"{TWO}"),
    };
    const auto result = opena8dj::proaudio::SelectOpenA8DJAggregatePair(endpoints);
    Expect(result.status == EndpointSelectionStatus::NoCommonIdentity,
           "different device containers must never be paired");
}

void TestContradictoryContainerOverridesMatchingInstanceId()
{
    auto render = Endpoint(EndpointDirection::Render, L"Audio 8 DJ (8ch Out)", L"{ONE}");
    auto capture = Endpoint(EndpointDirection::Capture, L"Audio 8 DJ (8ch In)", L"{TWO}");
    render.instanceId = L"SAME-INSTANCE";
    capture.instanceId = L"SAME-INSTANCE";
    const auto result = opena8dj::proaudio::SelectOpenA8DJAggregatePair({render, capture});
    Expect(result.status == EndpointSelectionStatus::NoCommonIdentity,
           "contradictory present ContainerIds must reject even when instance IDs match");
}

void TestStaleDuplicateIsRejected()
{
    std::vector<EndpointDescriptor> endpoints{
        Endpoint(EndpointDirection::Render, L"Audio 8 DJ (8ch Out)", L"{A8DJ}"),
        Endpoint(EndpointDirection::Render, L"Audio 8 DJ (8ch Out)", L"{A8DJ}"),
        Endpoint(EndpointDirection::Capture, L"Audio 8 DJ (8ch In)", L"{A8DJ}"),
    };
    const auto result = opena8dj::proaudio::SelectOpenA8DJAggregatePair(endpoints);
    Expect(result.status == EndpointSelectionStatus::AmbiguousRender,
           "duplicate active Render A endpoints must fail closed");
}

void TestInactiveAndWrongChannelEndpointsAreIgnored()
{
    std::vector<EndpointDescriptor> endpoints{
        Endpoint(EndpointDirection::Render, L"Audio 8 DJ (8ch Out)", L"{A8DJ}", 8, false),
        Endpoint(EndpointDirection::Capture, L"Audio 8 DJ (8ch In)", L"{A8DJ}"),
        Endpoint(EndpointDirection::Render, L"Audio 8 DJ (8ch Out)", L"{A8DJ}", 2),
    };
    const auto result = opena8dj::proaudio::SelectOpenA8DJAggregatePair(endpoints);
    Expect(result.status == EndpointSelectionStatus::Selected,
           "shared-mode mix channel count must not gate an exclusively validated endpoint");
}

void TestInterfacePropertyFallback()
{
    EndpointDescriptor render = Endpoint(EndpointDirection::Render, L"", L"{A8DJ}");
    EndpointDescriptor capture = Endpoint(EndpointDirection::Capture, L"", L"{A8DJ}");
    render.interfaceFriendlyName = L"OpenA8DJ Render A v2";
    capture.interfaceFriendlyName = L"OpenA8DJ Capture A v2";
    const auto result = opena8dj::proaudio::SelectOpenA8DJAggregatePair({render, capture});
    Expect(result.status == EndpointSelectionStatus::Selected,
           "interface properties should identify aggregate endpoints without display names");
}

void TestWindowsCompositeFriendlyName()
{
    const auto render = Endpoint(
        EndpointDirection::Render,
        L"Audio 8 DJ (8ch Out) (Audio 8 DJ)",
        L"{A8DJ}");
    const auto capture = Endpoint(
        EndpointDirection::Capture,
        L"Audio 8 DJ (8ch In) (Audio 8 DJ)",
        L"{A8DJ}");
    const auto result = opena8dj::proaudio::SelectOpenA8DJAggregatePair({render, capture});
    Expect(result.status == EndpointSelectionStatus::Selected,
           "Windows composite endpoint names should retain their aggregate role");
}

void TestOpaqueEndpointIdDoesNotAssignRole()
{
    auto render = Endpoint(EndpointDirection::Render, L"Unrelated output", L"{A8DJ}");
    auto capture = Endpoint(EndpointDirection::Capture, L"Unrelated input", L"{A8DJ}");
    render.endpointId = L"opaque-value-containing-opena8djrendera";
    capture.endpointId = L"opaque-value-containing-opena8djcapturea";
    const auto result = opena8dj::proaudio::SelectOpenA8DJAggregatePair({render, capture});
    Expect(result.status == EndpointSelectionStatus::MissingRender,
           "opaque IMMDevice endpoint IDs must never be parsed to assign endpoint roles");
}

void TestProviderCannotAuthorizeUnverifiedContainer()
{
    auto render = Endpoint(EndpointDirection::Render, L"Audio 8 DJ (8ch Out)", L"{A8DJ}");
    auto capture = Endpoint(EndpointDirection::Capture, L"Audio 8 DJ (8ch In)", L"{A8DJ}");
    render.containerVerifiedOpenA8DJUsb = false;
    capture.containerVerifiedOpenA8DJUsb = false;
    render.driverProvider = L"OpenA8DJ";
    capture.driverProvider = L"OpenA8DJ";
    const auto result = opena8dj::proaudio::SelectOpenA8DJAggregatePair({render, capture});
    Expect(result.status == EndpointSelectionStatus::UnverifiedIdentity,
           "provider strings must not authorize an unverified endpoint container");
}

void TestVerifiedContainerDoesNotRequireEndpointProviderProperty()
{
    auto render = Endpoint(EndpointDirection::Render, L"Audio 8 DJ (8ch Out)", L"{A8DJ}");
    auto capture = Endpoint(EndpointDirection::Capture, L"Audio 8 DJ (8ch In)", L"{A8DJ}");
    render.driverProvider.clear();
    capture.driverProvider.clear();
    const auto result = opena8dj::proaudio::SelectOpenA8DJAggregatePair({render, capture});
    Expect(result.status == EndpointSelectionStatus::Selected,
           "verified ContainerId should remain authoritative when endpoint provider is absent");
}

void TestAmbiguousCaptureIsRejected()
{
    std::vector<EndpointDescriptor> endpoints{
        Endpoint(EndpointDirection::Render, L"Audio 8 DJ (8ch Out)", L"{A8DJ}"),
        Endpoint(EndpointDirection::Capture, L"Audio 8 DJ (8ch In)", L"{A8DJ}"),
        Endpoint(EndpointDirection::Capture, L"Audio 8 DJ (8ch In)", L"{A8DJ}"),
    };
    const auto result = opena8dj::proaudio::SelectOpenA8DJAggregatePair(endpoints);
    Expect(result.status == EndpointSelectionStatus::AmbiguousCapture,
           "duplicate verified Capture A endpoints must fail closed");
}

}  // namespace

int main()
{
    TestUniquePair();
    TestIdentityMismatch();
    TestContradictoryContainerOverridesMatchingInstanceId();
    TestStaleDuplicateIsRejected();
    TestInactiveAndWrongChannelEndpointsAreIgnored();
    TestInterfacePropertyFallback();
    TestWindowsCompositeFriendlyName();
    TestOpaqueEndpointIdDoesNotAssignRole();
    TestProviderCannotAuthorizeUnverifiedContainer();
    TestVerifiedContainerDoesNotRequireEndpointProviderProperty();
    TestAmbiguousCaptureIsRejected();
    if (failures != 0) {
        std::cerr << failures << " selection contract test(s) failed\n";
        return EXIT_FAILURE;
    }
    std::cout << "PASS: passive pro-audio endpoint selection contracts\n";
    return EXIT_SUCCESS;
}
