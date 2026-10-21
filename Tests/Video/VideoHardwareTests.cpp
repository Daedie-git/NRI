// Copyright (c) 2026 NVIDIA Corporation

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstring>
#include <string>
#include <vector>

#ifdef _WIN32
#    include <windows.h>
#else
#    include <dlfcn.h>
#endif

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

struct MessageCounter {
    uint32_t errorNum = 0;
    std::string lastError;
};

void NRI_CALL CountMessage(nri::Message messageType, const char*, uint32_t, const char* message, void* userArg) {
    if (messageType != nri::Message::ERROR)
        return;

    MessageCounter& counter = *(MessageCounter*)userArg;
    counter.errorNum++;
    counter.lastError = message;
}

// Errors are counted by "CountMessage", the default handler would raise SIGTRAP
void NRI_CALL IgnoreAbort(void*) {
}

// Mirrors "DeviceVK::FilterInstanceLayers": NRI enables the Khronos validation layer only if the loader enumerates it
bool IsVulkanValidationLayerAvailable() {
    struct LayerProperties { // "VkLayerProperties"
        char layerName[256];
        uint32_t specVersion;
        uint32_t implementationVersion;
        char description[256];
    };

    using EnumerateInstanceLayerProperties = int32_t(
#ifdef _WIN32
        __stdcall
#endif
        *)(uint32_t*, LayerProperties*);

#ifdef _WIN32
    HMODULE loader = LoadLibraryA("vulkan-1.dll");
    EnumerateInstanceLayerProperties enumerate = loader ? (EnumerateInstanceLayerProperties)GetProcAddress(loader, "vkEnumerateInstanceLayerProperties") : nullptr;
#else
    void* loader = dlopen("libvulkan.so.1", RTLD_NOW | RTLD_LOCAL);
    EnumerateInstanceLayerProperties enumerate = loader ? (EnumerateInstanceLayerProperties)dlsym(loader, "vkEnumerateInstanceLayerProperties") : nullptr;
#endif

    bool found = false;
    uint32_t layerNum = 0;
    if (enumerate && enumerate(&layerNum, nullptr) == 0) {
        std::vector<LayerProperties> layers(layerNum);
        if (enumerate(&layerNum, layers.data()) == 0) {
            for (uint32_t i = 0; i < layerNum && !found; i++)
                found = std::strcmp(layers[i].layerName, "VK_LAYER_KHRONOS_validation") == 0;
        }
    }

#ifdef _WIN32
    if (loader)
        FreeLibrary(loader);
#else
    if (loader)
        dlclose(loader);
#endif

    return found;
}

struct ValidatedVideoDevice {
    MessageCounter messages;
    DeviceOwner owner;
    nri::VideoInterface video = {};
    nri::GraphicsAPI graphicsApi = nri::GraphicsAPI::NONE;
};

// Creates a video device with NRI and graphics API validation whose error messages are counted, or skips the test
void CreateValidatedVideoDevice(ValidatedVideoDevice& device) {
    uint32_t adapterNum = 0;
    REQUIRE(nri::nriEnumerateAdapters(nullptr, adapterNum) == nri::Result::SUCCESS);

    if (!adapterNum)
        SKIP("No graphics adapters are available");

    std::vector<nri::AdapterDesc> adapters(adapterNum);
    REQUIRE(nri::nriEnumerateAdapters(adapters.data(), adapterNum) == nri::Result::SUCCESS);

    const nri::AdapterDesc* selectedAdapter = FindVideoAdapter(adapters, device.graphicsApi);

    if (!selectedAdapter)
        SKIP("No adapter exposes NRI video queues");

    std::array<nri::QueueFamilyDesc, 3> queueFamilies = {};
    const uint32_t queueFamilyNum = FillVideoQueueFamilies(*selectedAdapter, queueFamilies);

    nri::DeviceCreationDesc creationDesc = {};
    creationDesc.graphicsAPI = device.graphicsApi;
    creationDesc.adapterDesc = selectedAdapter;
    creationDesc.queueFamilies = queueFamilies.data();
    creationDesc.queueFamilyNum = queueFamilyNum;
    creationDesc.callbackInterface.MessageCallback = CountMessage;
    creationDesc.callbackInterface.AbortExecution = IgnoreAbort;
    creationDesc.callbackInterface.userArg = &device.messages;
    creationDesc.enableNRIValidation = true;
    creationDesc.enableGraphicsAPIValidation = true;

    REQUIRE(nri::nriCreateDevice(creationDesc, device.owner.device) == nri::Result::SUCCESS);

    const nri::Result interfaceResult = nri::nriGetInterface(*device.owner.device, "VideoInterface", sizeof(device.video), &device.video);

    if (interfaceResult == nri::Result::UNSUPPORTED)
        SKIP("The selected adapter exposes no video interface");
    REQUIRE(interfaceResult == nri::Result::SUCCESS);
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

TEST_CASE("VID-HW-003 H.265 session parameters accept every Table A.8 level without native validation errors", "[video][hardware][regression][short]") {
    ValidatedVideoDevice device;
    CreateValidatedVideoDevice(device);

    // Every "general_level_idc" of H.265 Table A.8
    constexpr uint8_t levels[] = {30, 60, 63, 90, 93, 120, 123, 150, 153, 156, 180, 183, 186};

    uint32_t testedSessionNum = 0;

    for (nri::VideoSessionType type : {nri::VideoSessionType::DECODE, nri::VideoSessionType::ENCODE}) {
        nri::VideoSessionDesc sessionDesc = {};
        sessionDesc.type = type;
        sessionDesc.codec = nri::VideoCodec::H265;
        sessionDesc.format = nri::Format::NV12_UNORM;
        sessionDesc.width = 1920;
        sessionDesc.height = 1080;
        sessionDesc.maxReferenceNum = 4;
        nri::VideoCapabilities capabilities = {};
        const nri::Result capabilitiesResult = device.video.GetVideoCapabilities(*device.owner.device, sessionDesc, capabilities);

        if (capabilitiesResult == nri::Result::UNSUPPORTED)
            continue;
        REQUIRE(capabilitiesResult == nri::Result::SUCCESS);

        nri::VideoSession* session = nullptr;
        REQUIRE(device.video.CreateVideoSession(*device.owner.device, sessionDesc, session) == nri::Result::SUCCESS);

        nri::VideoH265VideoParameterSetDesc vps = {};
        vps.flags = nri::VideoH265VideoParameterSetBits::TEMPORAL_ID_NESTING;
        vps.profileTierLevel.generalProfileIdc = 1;
        vps.decPicBufMgr.maxDecPicBufferingMinus1[0] = 4;
        vps.decPicBufMgr.maxNumReorderPics[0] = 1;

        nri::VideoH265SequenceParameterSetDesc sps = {};
        sps.flags = nri::VideoH265SequenceParameterSetBits::TEMPORAL_ID_NESTING | nri::VideoH265SequenceParameterSetBits::SAMPLE_ADAPTIVE_OFFSET_ENABLED;
        sps.chromaFormatIdc = 1;
        sps.pictureWidthInLumaSamples = sessionDesc.width;
        sps.pictureHeightInLumaSamples = sessionDesc.height;
        sps.log2MaxPictureOrderCountLsbMinus4 = 4;
        sps.log2DiffMaxMinLumaCodingBlockSize = 2;
        sps.log2DiffMaxMinLumaTransformBlockSize = 2;
        sps.maxTransformHierarchyDepthInter = 2;
        sps.maxTransformHierarchyDepthIntra = 2;
        sps.decPicBufMgr = vps.decPicBufMgr;

        nri::VideoH265PictureParameterSetDesc pps = {};
        pps.flags = nri::VideoH265PictureParameterSetBits::LOOP_FILTER_ACROSS_SLICES_ENABLED;
        pps.log2ParallelMergeLevelMinus2 = 2;

        nri::VideoH265SessionParametersDesc h265 = {};
        h265.videoParameterSets = &vps;
        h265.videoParameterSetNum = 1;
        h265.sequenceParameterSets = &sps;
        h265.sequenceParameterSetNum = 1;
        h265.pictureParameterSets = &pps;
        h265.pictureParameterSetNum = 1;

        nri::VideoSessionParametersDesc parametersDesc = {};
        parametersDesc.session = session;
        parametersDesc.h265Parameters = &h265;

        for (uint8_t generalLevelIdc : levels) {
            CAPTURE((uint32_t)type, (uint32_t)generalLevelIdc);
            vps.profileTierLevel.generalLevelIdc = generalLevelIdc;
            sps.profileTierLevel = vps.profileTierLevel;

            device.messages = {};
            nri::VideoSessionParameters* parameters = nullptr;
            REQUIRE(device.video.CreateVideoSessionParameters(*device.owner.device, parametersDesc, parameters) == nri::Result::SUCCESS);
            REQUIRE(parameters != nullptr);
            device.video.DestroyVideoSessionParameters(parameters);

            INFO(device.messages.lastError);
            REQUIRE(device.messages.errorNum == 0);
        }

        vps.profileTierLevel.generalLevelIdc = 0;
        sps.profileTierLevel = vps.profileTierLevel;
        device.messages = {};
        nri::VideoSessionParameters* parameters = nullptr;
        REQUIRE(device.video.CreateVideoSessionParameters(*device.owner.device, parametersDesc, parameters) == nri::Result::INVALID_ARGUMENT);
        REQUIRE(parameters == nullptr);
        REQUIRE(device.messages.errorNum != 0); // the message callback observes errors

        device.video.DestroyVideoSession(session);
        testedSessionNum++;
    }

    if (!testedSessionNum)
        SKIP("The selected adapter supports neither H.265 decode nor encode");

    // Without the native layer "errorNum == 0" proves nothing about the native parameter sets
    if (device.graphicsApi == nri::GraphicsAPI::VK && !IsVulkanValidationLayerAvailable())
        SKIP("VK_LAYER_KHRONOS_validation is unavailable, native validation was not exercised");
}

TEST_CASE("VID-HW-004 H.264 session parameters accept every Table A-1 level without native validation errors", "[video][hardware][regression][short]") {
    ValidatedVideoDevice device;
    CreateValidatedVideoDevice(device);

    // Every "level_idc" of H.264 Table A-1, except level 1b
    constexpr uint8_t levels[] = {10, 11, 12, 13, 20, 21, 22, 30, 31, 32, 40, 41, 42, 50, 51, 52, 60, 61, 62};

    uint32_t testedSessionNum = 0;

    for (nri::VideoSessionType type : {nri::VideoSessionType::DECODE, nri::VideoSessionType::ENCODE}) {
        nri::VideoSessionDesc sessionDesc = {};
        sessionDesc.type = type;
        sessionDesc.codec = nri::VideoCodec::H264;
        sessionDesc.format = nri::Format::NV12_UNORM;
        sessionDesc.width = 1920;
        sessionDesc.height = 1088;
        sessionDesc.maxReferenceNum = 4;
        nri::VideoCapabilities capabilities = {};
        const nri::Result capabilitiesResult = device.video.GetVideoCapabilities(*device.owner.device, sessionDesc, capabilities);

        if (capabilitiesResult == nri::Result::UNSUPPORTED)
            continue;
        REQUIRE(capabilitiesResult == nri::Result::SUCCESS);

        nri::VideoSession* session = nullptr;
        REQUIRE(device.video.CreateVideoSession(*device.owner.device, sessionDesc, session) == nri::Result::SUCCESS);

        nri::VideoH264SequenceParameterSetDesc sps = {};
        sps.flags = nri::VideoH264SequenceParameterSetBits::DIRECT_8X8_INFERENCE | nri::VideoH264SequenceParameterSetBits::FRAME_MBS_ONLY;
        sps.profileIdc = 100;
        sps.chromaFormatIdc = 1;
        sps.log2MaxFrameNumMinus4 = 4;
        sps.log2MaxPictureOrderCountLsbMinus4 = 4;
        sps.referenceFrameNum = 4;
        sps.pictureWidthInMbsMinus1 = (uint16_t)(sessionDesc.width / 16 - 1);
        sps.pictureHeightInMapUnitsMinus1 = (uint16_t)(sessionDesc.height / 16 - 1);

        nri::VideoH264PictureParameterSetDesc pps = {};
        pps.flags = nri::VideoH264PictureParameterSetBits::DEBLOCKING_FILTER_CONTROL_PRESENT;

        nri::VideoH264SessionParametersDesc h264 = {};
        h264.sequenceParameterSets = &sps;
        h264.sequenceParameterSetNum = 1;
        h264.pictureParameterSets = &pps;
        h264.pictureParameterSetNum = 1;

        nri::VideoSessionParametersDesc parametersDesc = {};
        parametersDesc.session = session;
        parametersDesc.h264Parameters = &h264;

        for (uint8_t levelIdc : levels) {
            CAPTURE((uint32_t)type, (uint32_t)levelIdc);
            sps.levelIdc = levelIdc;

            device.messages = {};
            nri::VideoSessionParameters* parameters = nullptr;
            REQUIRE(device.video.CreateVideoSessionParameters(*device.owner.device, parametersDesc, parameters) == nri::Result::SUCCESS);
            REQUIRE(parameters != nullptr);
            device.video.DestroyVideoSessionParameters(parameters);

            INFO(device.messages.lastError);
            REQUIRE(device.messages.errorNum == 0);
        }

        sps.levelIdc = 0;
        device.messages = {};
        nri::VideoSessionParameters* parameters = nullptr;
        REQUIRE(device.video.CreateVideoSessionParameters(*device.owner.device, parametersDesc, parameters) == nri::Result::INVALID_ARGUMENT);
        REQUIRE(parameters == nullptr);
        REQUIRE(device.messages.errorNum != 0); // the message callback observes errors

        device.video.DestroyVideoSession(session);
        testedSessionNum++;
    }

    if (!testedSessionNum)
        SKIP("The selected adapter supports neither H.264 decode nor encode");

    // Without the native layer "errorNum == 0" proves nothing about the native parameter sets
    if (device.graphicsApi == nri::GraphicsAPI::VK && !IsVulkanValidationLayerAvailable())
        SKIP("VK_LAYER_KHRONOS_validation is unavailable, native validation was not exercised");
}
