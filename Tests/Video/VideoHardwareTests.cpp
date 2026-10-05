// Copyright (c) 2026 NVIDIA Corporation

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <vector>

#include "NRI.h"

#include "Extensions/NRIDeviceCreation.h"
#include "Extensions/NRIVideo.h"

#if NRI_ENABLE_D3D12_SUPPORT
constexpr bool NRI_VIDEO_HAS_D3D12_SUPPORT = true;
#else
constexpr bool NRI_VIDEO_HAS_D3D12_SUPPORT = false;
#endif

#if NRI_ENABLE_VK_SUPPORT
constexpr bool NRI_VIDEO_HAS_VK_SUPPORT = true;
#else
constexpr bool NRI_VIDEO_HAS_VK_SUPPORT = false;
#endif

static_assert(NRI_VIDEO_HAS_D3D12_SUPPORT == NRI_VIDEO_EXPECT_D3D12_SUPPORT);
static_assert(NRI_VIDEO_HAS_VK_SUPPORT == NRI_VIDEO_EXPECT_VK_SUPPORT);

namespace {

struct DeviceOwner {
    ~DeviceOwner() {
        if (device)
            nri::nriDestroyDevice(device);
    }

    nri::Device* device = nullptr;
};

nri::GraphicsAPI GetHardwareApi(const nri::AdapterDesc& adapter) {
    if (adapter.supportedGraphicsAPIs & nri::GraphicsAPI::VK)
        return nri::GraphicsAPI::VK;

    if (adapter.supportedGraphicsAPIs & nri::GraphicsAPI::D3D12)
        return nri::GraphicsAPI::D3D12;

    return nri::GraphicsAPI::NONE;
}

const nri::AdapterDesc* FindVideoAdapter(const std::vector<nri::AdapterDesc>& adapters, nri::GraphicsAPI& graphicsApi) {
    for (const nri::AdapterDesc& adapter : adapters) {
        const nri::GraphicsAPI candidateApi = GetHardwareApi(adapter);

        if (candidateApi != nri::GraphicsAPI::NONE && (adapter.queueNum[(uint32_t)nri::QueueType::VIDEO_DECODE] || adapter.queueNum[(uint32_t)nri::QueueType::VIDEO_ENCODE])) {
            graphicsApi = candidateApi;

            return &adapter;
        }
    }

    return nullptr;
}

uint32_t FillVideoQueueFamilies(const nri::AdapterDesc& adapter, std::array<nri::QueueFamilyDesc, 3>& queueFamilies) {
    uint32_t queueFamilyNum = 0;
    queueFamilies[queueFamilyNum].queueNum = 1;
    queueFamilies[queueFamilyNum].queueType = nri::QueueType::GRAPHICS;
    queueFamilyNum++;

    for (nri::QueueType queueType : {nri::QueueType::VIDEO_DECODE, nri::QueueType::VIDEO_ENCODE}) {
        if (adapter.queueNum[(uint32_t)queueType]) {
            queueFamilies[queueFamilyNum].queueNum = 1;
            queueFamilies[queueFamilyNum].queueType = queueType;
            queueFamilyNum++;
        }
    }

    return queueFamilyNum;
}

} // namespace

TEST_CASE("VID-HW-001 video adapters create queues and expose the interface", "[video][hardware][short]") {
    uint32_t adapterNum = 0;
    REQUIRE(nri::nriEnumerateAdapters(nullptr, adapterNum) == nri::Result::SUCCESS);

    if (!adapterNum)
        SKIP("No graphics adapters are available");

    std::vector<nri::AdapterDesc> adapters(adapterNum);
    REQUIRE(nri::nriEnumerateAdapters(adapters.data(), adapterNum) == nri::Result::SUCCESS);

    nri::GraphicsAPI graphicsApi = nri::GraphicsAPI::NONE;
    const nri::AdapterDesc* selectedAdapter = FindVideoAdapter(adapters, graphicsApi);

    if (!selectedAdapter)
        SKIP("No adapter exposes NRI video queues");

    std::array<nri::QueueFamilyDesc, 3> queueFamilies = {};
    const uint32_t queueFamilyNum = FillVideoQueueFamilies(*selectedAdapter, queueFamilies);

    nri::DeviceCreationDesc creationDesc = {};
    creationDesc.graphicsAPI = graphicsApi;
    creationDesc.adapterDesc = selectedAdapter;
    creationDesc.queueFamilies = queueFamilies.data();
    creationDesc.queueFamilyNum = queueFamilyNum;
    creationDesc.enableNRIValidation = true;

    DeviceOwner owner;
    REQUIRE(nri::nriCreateDevice(creationDesc, owner.device) == nri::Result::SUCCESS);

    nri::CoreInterface core = {};
    REQUIRE(nri::nriGetInterface(*owner.device, "CoreInterface", sizeof(core), &core) == nri::Result::SUCCESS);
    nri::VideoInterface video = {};
    REQUIRE(nri::nriGetInterface(*owner.device, "VideoInterface", sizeof(video), &video) == nri::Result::SUCCESS);

    for (nri::QueueType queueType : {nri::QueueType::VIDEO_DECODE, nri::QueueType::VIDEO_ENCODE}) {
        if (!selectedAdapter->queueNum[(uint32_t)queueType])
            continue;

        nri::Queue* queue = nullptr;
        REQUIRE(core.GetQueue(*owner.device, queueType, 0, queue) == nri::Result::SUCCESS);
        REQUIRE(queue != nullptr);

        nri::CommandAllocator* allocator = nullptr;
        REQUIRE(core.CreateCommandAllocator(*queue, allocator) == nri::Result::SUCCESS);
        nri::CommandBuffer* commandBuffer = nullptr;
        REQUIRE(core.CreateCommandBuffer(*allocator, commandBuffer) == nri::Result::SUCCESS);
        REQUIRE(core.BeginCommandBuffer(*commandBuffer, nullptr) == nri::Result::SUCCESS);
        REQUIRE(core.EndCommandBuffer(*commandBuffer) == nri::Result::SUCCESS);
        core.DestroyCommandBuffer(commandBuffer);
        core.DestroyCommandAllocator(allocator);
    }
}

TEST_CASE("VID-HW-002 video capability matrix is internally consistent", "[video][hardware][short]") {
    uint32_t adapterNum = 0;
    REQUIRE(nri::nriEnumerateAdapters(nullptr, adapterNum) == nri::Result::SUCCESS);

    if (!adapterNum)
        SKIP("No graphics adapters are available");

    std::vector<nri::AdapterDesc> adapters(adapterNum);
    REQUIRE(nri::nriEnumerateAdapters(adapters.data(), adapterNum) == nri::Result::SUCCESS);

    nri::GraphicsAPI graphicsApi = nri::GraphicsAPI::NONE;
    const nri::AdapterDesc* selectedAdapter = FindVideoAdapter(adapters, graphicsApi);

    if (!selectedAdapter)
        SKIP("No adapter exposes NRI video queues");

    std::array<nri::QueueFamilyDesc, 3> queueFamilies = {};
    const uint32_t queueFamilyNum = FillVideoQueueFamilies(*selectedAdapter, queueFamilies);

    nri::DeviceCreationDesc creationDesc = {};
    creationDesc.graphicsAPI = graphicsApi;
    creationDesc.adapterDesc = selectedAdapter;
    creationDesc.queueFamilies = queueFamilies.data();
    creationDesc.queueFamilyNum = queueFamilyNum;

    DeviceOwner owner;
    REQUIRE(nri::nriCreateDevice(creationDesc, owner.device) == nri::Result::SUCCESS);

    nri::VideoInterface video = {};
    const nri::Result interfaceResult = nri::nriGetInterface(*owner.device, "VideoInterface", sizeof(video), &video);

    if (interfaceResult == nri::Result::UNSUPPORTED)
        SKIP("The selected adapter exposes no video interface");
    REQUIRE(interfaceResult == nri::Result::SUCCESS);

    uint32_t supportedCombinationNum = 0;

    for (nri::VideoSessionType type : {nri::VideoSessionType::DECODE, nri::VideoSessionType::ENCODE}) {
        for (nri::VideoCodec codec : {nri::VideoCodec::H264, nri::VideoCodec::H265, nri::VideoCodec::AV1}) {
            nri::VideoSessionDesc desc = {};
            desc.type = type;
            desc.codec = codec;
            desc.format = nri::Format::NV12_UNORM;
            desc.width = 1920;
            desc.height = 1080;
            desc.maxReferenceNum = 4;
            nri::VideoCapabilities capabilities = {};
            const nri::Result result = video.GetVideoCapabilities(*owner.device, desc, capabilities);

            UNSCOPED_INFO("type=" << (uint32_t)type << ", codec=" << (uint32_t)codec << ", result=" << (int32_t)result
                                  << ", extent=" << capabilities.widthMin << "x" << capabilities.heightMin << "-" << capabilities.widthMax << "x" << capabilities.heightMax
                                  << ", granularity=" << capabilities.pictureAccessGranularityWidth << "x" << capabilities.pictureAccessGranularityHeight);

            if (result == nri::Result::UNSUPPORTED)
                continue;

            REQUIRE(result == nri::Result::SUCCESS);
            REQUIRE(capabilities.widthMin != 0);
            REQUIRE(capabilities.heightMin != 0);
            REQUIRE(capabilities.widthMin <= capabilities.widthMax);
            REQUIRE(capabilities.heightMin <= capabilities.heightMax);
            REQUIRE(capabilities.bitstreamOffsetAlignment != 0);
            REQUIRE(capabilities.bitstreamSizeAlignment != 0);

            nri::VideoSession* session = nullptr;
            REQUIRE(video.CreateVideoSession(*owner.device, desc, session) == nri::Result::SUCCESS);
            REQUIRE(session != nullptr);
            video.DestroyVideoSession(session);
            supportedCombinationNum++;
        }
    }

#if NRI_VIDEO_REQUIRE_FULL_CODEC_MATRIX
    REQUIRE(supportedCombinationNum == 6);
#else
    REQUIRE(supportedCombinationNum != 0);
#endif
}
