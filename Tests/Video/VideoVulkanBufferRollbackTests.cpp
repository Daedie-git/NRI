// Copyright (c) 2026 NVIDIA Corporation

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <functional>
#include <limits>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "vk_mem_alloc.h"

// The test replaces the private dispatch table, memory properties and allocator of "DeviceVK" with mocks
#define private public
#include "SharedVK.h"

#include "BufferVK.h"
#include "DeviceVK.h"
#undef private

namespace {

const VkBuffer kInitialBuffer = (VkBuffer)(uintptr_t)0x1111;
const VkBuffer kPoisonBuffer = (VkBuffer)(uintptr_t)0xBAD0BAD0;

struct MockState {
    uint32_t createBufferCalls = 0;
    std::vector<VkBuffer> destroyedBuffers;
    std::vector<VkBuffer> createdBuffers;
};

MockState g_State;

VKAPI_ATTR VkResult VKAPI_CALL MockCreateBuffer(VkDevice, const VkBufferCreateInfo*, const VkAllocationCallbacks*, VkBuffer* buffer) {
    g_State.createBufferCalls++;

    // 1st call is "BufferVK::Create", 2nd call is made by "vmaCreateBuffer": fails and leaves an undefined handle behind
    if (g_State.createBufferCalls == 1) {
        *buffer = kInitialBuffer;
        g_State.createdBuffers.push_back(*buffer);

        return VK_SUCCESS;
    }

    *buffer = kPoisonBuffer;

    return VK_ERROR_OUT_OF_DEVICE_MEMORY;
}

VKAPI_ATTR void VKAPI_CALL MockDestroyBuffer(VkDevice, VkBuffer buffer, const VkAllocationCallbacks*) {
    if (buffer)
        g_State.destroyedBuffers.push_back(buffer);
}

VKAPI_ATTR void VKAPI_CALL MockGetBufferMemoryRequirements(VkDevice, VkBuffer, VkMemoryRequirements* requirements) {
    *requirements = {};
    requirements->size = 256;
    requirements->alignment = 16;
    requirements->memoryTypeBits = 1;
}

VKAPI_ATTR void VKAPI_CALL MockGetBufferMemoryRequirements2(VkDevice device, const VkBufferMemoryRequirementsInfo2* info, VkMemoryRequirements2* requirements) {
    MockGetBufferMemoryRequirements(device, info->buffer, &requirements->memoryRequirements);
}

VKAPI_ATTR void VKAPI_CALL MockGetPhysicalDeviceProperties(VkPhysicalDevice, VkPhysicalDeviceProperties* properties) {
    *properties = {};
    properties->apiVersion = VK_API_VERSION_1_0;
    properties->limits.maxMemoryAllocationCount = 4096;
    properties->limits.bufferImageGranularity = 1;
    properties->limits.nonCoherentAtomSize = 1;
}

VKAPI_ATTR void VKAPI_CALL MockGetPhysicalDeviceMemoryProperties(VkPhysicalDevice, VkPhysicalDeviceMemoryProperties* properties) {
    *properties = {};
    properties->memoryTypeCount = 1;
    properties->memoryTypes[0].propertyFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    properties->memoryHeapCount = 1;
    properties->memoryHeaps[0].size = 1ull << 30;
    properties->memoryHeaps[0].flags = VK_MEMORY_HEAP_DEVICE_LOCAL_BIT;
}

VKAPI_ATTR void VKAPI_CALL Unreachable() {
    std::abort();
}

VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL MockGetProcAddr(VkInstance, const char*) {
    return nullptr;
}

VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL MockGetDeviceProcAddr(VkDevice, const char*) {
    return nullptr;
}

static void NRI_CALL IgnoreMessage(nri::Message, const char*, uint32_t, const char*, void*) {
}

struct BufferRollbackHarness {
    BufferRollbackHarness()
        : device(callbacks, allocationCallbacks) {
        g_State = {};

        device.m_IsSupported.videoMaintenance1 = true;
        device.m_VK.CreateBuffer = MockCreateBuffer;
        device.m_VK.DestroyBuffer = MockDestroyBuffer;
        device.m_VK.GetBufferMemoryRequirements2 = MockGetBufferMemoryRequirements2;
        MockGetPhysicalDeviceMemoryProperties(nullptr, &device.m_MemoryProps);

        VmaVulkanFunctions functions = {};
        // VMA never reaches these before the failing "vkCreateBuffer"; any call crashes loudly
        functions.vkGetInstanceProcAddr = MockGetProcAddr;
        functions.vkGetDeviceProcAddr = MockGetDeviceProcAddr;
        functions.vkAllocateMemory = (PFN_vkAllocateMemory)Unreachable;
        functions.vkFreeMemory = (PFN_vkFreeMemory)Unreachable;
        functions.vkMapMemory = (PFN_vkMapMemory)Unreachable;
        functions.vkUnmapMemory = (PFN_vkUnmapMemory)Unreachable;
        functions.vkFlushMappedMemoryRanges = (PFN_vkFlushMappedMemoryRanges)Unreachable;
        functions.vkInvalidateMappedMemoryRanges = (PFN_vkInvalidateMappedMemoryRanges)Unreachable;
        functions.vkBindBufferMemory = (PFN_vkBindBufferMemory)Unreachable;
        functions.vkBindImageMemory = (PFN_vkBindImageMemory)Unreachable;
        functions.vkGetImageMemoryRequirements = (PFN_vkGetImageMemoryRequirements)Unreachable;
        functions.vkCreateImage = (PFN_vkCreateImage)Unreachable;
        functions.vkDestroyImage = (PFN_vkDestroyImage)Unreachable;
        functions.vkCmdCopyBuffer = (PFN_vkCmdCopyBuffer)Unreachable;
        functions.vkGetPhysicalDeviceProperties = MockGetPhysicalDeviceProperties;
        functions.vkGetPhysicalDeviceMemoryProperties = MockGetPhysicalDeviceMemoryProperties;
        functions.vkGetBufferMemoryRequirements = MockGetBufferMemoryRequirements;
        functions.vkCreateBuffer = MockCreateBuffer;
        functions.vkDestroyBuffer = MockDestroyBuffer;

        VmaAllocatorCreateInfo allocatorCreateInfo = {};
        allocatorCreateInfo.vulkanApiVersion = VK_API_VERSION_1_0;
        allocatorCreateInfo.pVulkanFunctions = &functions;
        REQUIRE(vmaCreateAllocator(&allocatorCreateInfo, &device.m_Vma) == VK_SUCCESS);
    }

    nri::AllocationCallbacks allocationCallbacks = {
        [](void*, size_t size, size_t) -> void* { return std::malloc(size); },
        [](void*, void* memory, size_t size, size_t) -> void* { return std::realloc(memory, size); },
        [](void*, void* memory) { std::free(memory); },
    };
    nri::CallbackInterface callbacks = {IgnoreMessage};
    nri::DeviceVK device;
};

} // namespace

TEST_CASE("VID-REG-032 Vulkan video buffer rollback never destroys an undefined native handle", "[video][vulkan][regression][short]") {
    BufferRollbackHarness harness;

    nri::BufferDesc desc = {};
    desc.size = 256;
    desc.usage = nri::BufferUsageBits::VIDEO_DECODE;

    nri::CoreInterface core = {};
    REQUIRE(harness.device.FillFunctionTable(core) == nri::Result::SUCCESS);

    nri::Buffer* buffer = (nri::Buffer*)&harness;
    const nri::Result result = core.CreateCommittedBuffer((nri::Device&)harness.device, nri::MemoryLocation::DEVICE, 0.5f, desc, buffer);

    REQUIRE(result != nri::Result::SUCCESS);
    REQUIRE(buffer == nullptr);
    REQUIRE(g_State.createBufferCalls == 2);

    // Only the initial valid buffer is destroyed, once, by the video-only path before "vmaCreateBuffer"
    REQUIRE(g_State.destroyedBuffers.size() == 1);
    REQUIRE(g_State.destroyedBuffers[0] == kInitialBuffer);
    REQUIRE(std::find(g_State.destroyedBuffers.begin(), g_State.destroyedBuffers.end(), kPoisonBuffer) == g_State.destroyedBuffers.end());
}
