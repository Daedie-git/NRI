// © 2026 NVIDIA Corporation

#pragma once

namespace nri {

struct BufferWGPU final : public DebugNameBase {
    inline BufferWGPU(DeviceWGPU& device)
        : m_Device(device)
        , m_CpuMemory(device.GetStdAllocator()) {
    }

    ~BufferWGPU();

    inline operator WGPUBuffer() const {
        return m_Buffer;
    }

    inline DeviceWGPU& GetDevice() const {
        return m_Device;
    }

    inline const BufferDesc& GetDesc() const {
        return m_Desc;
    }

    inline uint64_t GetSize() const {
        return m_Desc.size;
    }

    inline bool IsHostReadback() const {
        return m_MemoryLocation == MemoryLocation::HOST_READBACK;
    }

    Result Create(const BufferDesc& bufferDesc);
    Result Create(const BufferDesc& bufferDesc, MemoryLocation memoryLocation);
    void* Map(uint64_t offset, uint64_t size);
    // Internal synchronous host copies must not inherit the application's deferred-map policy.
    void* MapBlocking(uint64_t offset, uint64_t size);
    // Host-visible upload buffers are CPU shadows; GPU reads need their latest bytes queued first.
    void FlushHostWrites(uint64_t offset, uint64_t size);
    void Unmap();

    Result SetHostVisible(MemoryLocation memoryLocation);

    //================================================================================================================
    // DebugNameBase
    //================================================================================================================

    void SetDebugName(const char* name) NRI_DEBUG_NAME_OVERRIDE {
        MaybeUnused(name);
    }

private:
    Result CreateNativeBuffer();
    void* MapInternal(uint64_t offset, uint64_t size, bool blocking);

private:
    DeviceWGPU& m_Device;
    Vector<uint8_t> m_CpuMemory;
    WGPUBuffer m_Buffer = nullptr;
    BufferDesc m_Desc = {};
    const void* m_MappedReadback = nullptr;
#if defined(__EMSCRIPTEN__)
    // The first deferred Map starts a request; a later call returns its range after callback delivery.
    struct PendingMap;
    PendingMap* m_PendingMap = nullptr;
#endif
    uint64_t m_MapOffset = 0;
    uint64_t m_MapSize = 0;
    MemoryLocation m_MemoryLocation = MemoryLocation::DEVICE;
};

} // namespace nri
