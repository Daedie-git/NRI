// Copyright (c) 2026 NVIDIA Corporation

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstring>

#include "NRI.h"

#include "Extensions/NRIDeviceCreation.h"
#include "Extensions/NRIVideo.h"

namespace {

struct DeviceOwner {
    ~DeviceOwner() {
        if (device)
            nri::nriDestroyDevice(device);
    }

    nri::Device* device = nullptr;
};

void NRI_CALL IgnoreMessage(nri::Message, const char*, uint32_t, const char*, void*) {
}

void NRI_CALL IgnoreAbort(void*) {
}

void CreateNoneDevice(DeviceOwner& owner) {
    nri::AdapterDesc adapter = {};
    adapter.supportedGraphicsAPIs = nri::GraphicsAPI::NONE;
    adapter.queueNum[(uint32_t)nri::QueueType::GRAPHICS] = 1;

    nri::DeviceCreationDesc desc = {};
    desc.graphicsAPI = nri::GraphicsAPI::NONE;
    desc.adapterDesc = &adapter;
    desc.callbackInterface.MessageCallback = IgnoreMessage;
    desc.callbackInterface.AbortExecution = IgnoreAbort;

    REQUIRE(nri::nriCreateDevice(desc, owner.device) == nri::Result::SUCCESS);
}

} // namespace

TEST_CASE("VID-INT-001 VideoInterface export validates names, sizes, and outputs", "[video][integration][short]") {
    DeviceOwner owner;
    CreateNoneDevice(owner);

    for (const char* name : {"VideoInterface", "nri::VideoInterface", "NriVideoInterface"}) {
        nri::VideoInterface video;
        std::memset(&video, 0xA5, sizeof(video));
        REQUIRE(nri::nriGetInterface(*owner.device, name, sizeof(video), &video) == nri::Result::UNSUPPORTED);

        const std::array<uint8_t, sizeof(video)> zero = {};
        REQUIRE(std::memcmp(&video, zero.data(), sizeof(video)) == 0);
    }

    std::array<uint8_t, sizeof(nri::VideoInterface)> bytes = {};
    bytes.fill(0xA5);
    REQUIRE(nri::nriGetInterface(*owner.device, "VideoInterface", bytes.size() - 1, bytes.data()) == nri::Result::INVALID_ARGUMENT);
    REQUIRE(bytes.back() == 0xA5);
}
