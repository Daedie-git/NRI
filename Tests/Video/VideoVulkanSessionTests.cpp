// Copyright (c) 2026 NVIDIA Corporation

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <limits>

#include "vk_mem_alloc.h"

#include "SharedVK.h"

#include "BufferVK.h"
#include "DeviceVK.h"
#include "QueueVK.h"
#include "VideoHelpersVK.h"
#include "VideoSessionParametersVK.h"
#include "VideoSessionVK.h"

namespace {

struct AllocationHeader {
    void* memory;
    size_t size;
};

static void* NRI_CALL Allocate(void*, size_t size, size_t alignment) {
    const size_t effectiveAlignment = std::max(alignment, alignof(AllocationHeader));

    if ((effectiveAlignment & (effectiveAlignment - 1)) || effectiveAlignment > std::numeric_limits<size_t>::max() - sizeof(AllocationHeader) + 1)
        return nullptr;

    const size_t overhead = sizeof(AllocationHeader) + effectiveAlignment - 1;

    if (size > std::numeric_limits<size_t>::max() - overhead)
        return nullptr;

    uint8_t* memory = (uint8_t*)std::malloc(size + overhead);

    if (!memory)
        return nullptr;

    uint8_t* alignedMemory = nri::Align(memory + sizeof(AllocationHeader), effectiveAlignment);
    AllocationHeader* header = (AllocationHeader*)alignedMemory - 1;
    header->memory = memory;
    header->size = size;

    return alignedMemory;
}

static void* NRI_CALL Reallocate(void* userArg, void* memory, size_t size, size_t alignment) {
    if (!memory)
        return Allocate(userArg, size, alignment);

    AllocationHeader* oldHeader = (AllocationHeader*)memory - 1;
    void* newMemory = Allocate(userArg, size, alignment);

    if (!newMemory)
        return nullptr;

    std::memcpy(newMemory, memory, std::min(size, oldHeader->size));
    std::free(oldHeader->memory);

    return newMemory;
}

static void NRI_CALL Free(void*, void* memory) {
    if (memory)
        std::free(((AllocationHeader*)memory - 1)->memory);
}

static void NRI_CALL IgnoreMessage(nri::Message, const char*, uint32_t, const char*, void*) {
}

struct SessionHarness {
    SessionHarness()
        : device(callbacks, allocationCallbacks)
        , session(device) {
    }

    nri::AllocationCallbacks allocationCallbacks = {Allocate, Reallocate, Free};
    nri::CallbackInterface callbacks = {IgnoreMessage};
    nri::DeviceVK device;
    nri::VideoSessionVK session;
};

} // namespace

TEST_CASE("VID-VK-001 Vulkan session reset restores implicit first-use initialization", "[video][vulkan][short]") {
    SessionHarness harness;
    REQUIRE_FALSE(harness.session.IsResetRecorded());

    harness.session.SetResetRecorded();
    REQUIRE(harness.session.IsResetRecorded());

    harness.session.Reset();
    REQUIRE_FALSE(harness.session.IsResetRecorded());
}

TEST_CASE("VID-REG-009 Vulkan session reset releases abandoned feedback queries", "[video][vulkan][regression][short]") {
    SessionHarness harness;
    nri::BufferVK* metadata = (nri::BufferVK*)&harness;

    for (uint32_t i = 0; i < 64; i++)
        REQUIRE(harness.session.AllocateEncodeFeedbackQuery(metadata, i * 16) != UINT32_MAX);
    REQUIRE(harness.session.AllocateEncodeFeedbackQuery(metadata, 1024) == UINT32_MAX);
    REQUIRE(harness.session.FindEncodeFeedbackQuery(metadata, 0) != UINT32_MAX);

    harness.session.Reset();
    REQUIRE(harness.session.FindEncodeFeedbackQuery(metadata, 0) == UINT32_MAX);
    REQUIRE(harness.session.AllocateEncodeFeedbackQuery(metadata, 0) != UINT32_MAX);
}

TEST_CASE("VID-REG-025 Vulkan AV1 decode info requires an explicit payload header", "[video][vulkan][regression][short]") {
    SessionHarness harness;
    std::array<uint8_t, sizeof(nri::VideoEncodeFeedback) + sizeof(uint32_t) * 3> metadata = {};
    nri::DeviceDesc& deviceDesc = const_cast<nri::DeviceDesc&>(harness.device.GetDesc());
    deviceDesc.features.video = true;
    nri::VideoInterface video = {};
    REQUIRE(harness.device.FillFunctionTable(video) == nri::Result::SUCCESS);

    nri::BufferVK buffer(harness.device);
    nri::BufferVKDesc bufferDesc = {};
    bufferDesc.vkBuffer = 1;
    bufferDesc.size = metadata.size();
    bufferDesc.mappedMemory = metadata.data();
    REQUIRE(buffer.Create(bufferDesc) == nri::Result::SUCCESS);

    nri::VideoEncodeFeedback feedback = {};
    feedback.encodedBitstreamWrittenBytes = sizeof(uint32_t) * 3;
    feedback.writtenSubregionNum = 1;
    nri::VideoAV1SequenceDesc sequence = {};
    sequence.bitDepth = 8;
    nri::VideoAV1EncodeDecodeInfoDesc desc = {};
    desc.feedback = &feedback;
    desc.sequence = &sequence;
    nri::VideoAV1EncodeDecodeInfo info = {};

    REQUIRE(video.GetVideoAV1EncodeDecodeInfo((nri::VideoSession&)harness.session, (nri::Buffer&)buffer, 0, desc, info) == nri::Result::UNSUPPORTED);
}
