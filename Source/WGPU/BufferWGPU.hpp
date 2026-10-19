// © 2026 NVIDIA Corporation

#if defined(__EMSCRIPTEN__)
// Applications that map from a frame callback (which cannot suspend) opt in to deferred readback maps.
static bool g_NonBlockingReadbackMaps = false;
// Test hook (`?nomultidraw`): behave as a browser without multi-draw-indirect, as a stock Chrome does.
static bool g_DisableMultiDrawIndirect = false;
// Indirect draw commands recorded (page telemetry: a scene that never draws a mesh is not ready).
static uint64_t g_IndirectDrawCommands = 0;
extern "C" uint64_t nriWGPUIndirectDrawCommands() {
    return g_IndirectDrawCommands;
}
extern "C" void nriWGPUSetMultiDrawIndirectDisabled(bool disabled) {
    g_DisableMultiDrawIndirect = disabled;
}
extern "C" void nriWGPUSetNonBlockingReadbackMaps(bool enabled) {
    g_NonBlockingReadbackMaps = enabled;
}

struct BufferWGPU::PendingMap {
    bool completed = false;
    bool orphaned = false;
    WGPUMapAsyncStatus status = WGPUMapAsyncStatus_Error;
};
#endif

BufferWGPU::~BufferWGPU() {
#if defined(__EMSCRIPTEN__)
    // An in-flight callback frees its own context once its buffer is gone.
    if (m_PendingMap) {
        if (m_PendingMap->completed)
            delete m_PendingMap;
        else
            m_PendingMap->orphaned = true;
    }
#endif
    if (m_Buffer)
        wgpuBufferRelease(m_Buffer);
}

Result BufferWGPU::Create(const BufferDesc& bufferDesc) {
    return Create(bufferDesc, MemoryLocation::DEVICE);
}

Result BufferWGPU::Create(const BufferDesc& bufferDesc, MemoryLocation memoryLocation) {
    m_Desc = bufferDesc;
    m_MemoryLocation = memoryLocation;

    return CreateNativeBuffer();
}

Result BufferWGPU::CreateNativeBuffer() {
    uint64_t nativeSize = Align(std::max<uint64_t>(m_Desc.size, 4), 4);

    WGPUBufferDescriptor desc = WGPU_BUFFER_DESCRIPTOR_INIT;
    desc.size = nativeSize;
    desc.usage = m_MemoryLocation == MemoryLocation::HOST_READBACK ? WGPUBufferUsage_MapRead | WGPUBufferUsage_CopyDst : GetBufferUsage(m_Desc.usage);

    WGPUBuffer buffer = wgpuDeviceCreateBuffer(m_Device, &desc);
    if (!buffer)
        return Result::FAILURE;

    if (m_Buffer)
        wgpuBufferRelease(m_Buffer);

    m_Buffer = buffer;

    if (m_MemoryLocation != MemoryLocation::DEVICE && m_MemoryLocation != MemoryLocation::HOST_READBACK)
        m_CpuMemory.resize((size_t)nativeSize);
    else
        m_CpuMemory.clear();

    return Result::SUCCESS;
}

Result BufferWGPU::SetHostVisible(MemoryLocation memoryLocation) {
    if (m_MemoryLocation == memoryLocation)
        return Result::SUCCESS;

    MemoryLocation oldMemoryLocation = m_MemoryLocation;
    m_MemoryLocation = memoryLocation;

    if (m_Buffer && CreateNativeBuffer() != Result::SUCCESS) {
        m_MemoryLocation = oldMemoryLocation;
        return Result::FAILURE;
    }

    if (m_MemoryLocation != MemoryLocation::DEVICE && m_MemoryLocation != MemoryLocation::HOST_READBACK && m_CpuMemory.empty())
        m_CpuMemory.resize((size_t)Align(std::max<uint64_t>(m_Desc.size, 4), 4));

    return Result::SUCCESS;
}

void* BufferWGPU::Map(uint64_t offset, uint64_t size) {
    m_MapOffset = offset;
    m_MapSize = size == WHOLE_SIZE ? m_Desc.size - offset : size;

    if (m_MemoryLocation == MemoryLocation::HOST_READBACK) {
        size_t mapOffset = (size_t)(m_MapOffset & ~7ull);
        size_t mapSize = (size_t)Align(m_MapOffset + m_MapSize - mapOffset, 4);
#if defined(__EMSCRIPTEN__)
        if (!g_NonBlockingReadbackMaps) {
            WGPUBufferMapCallbackInfo callbackInfo = WGPU_BUFFER_MAP_CALLBACK_INFO_INIT;
            callbackInfo.mode = WGPUCallbackMode_WaitAnyOnly;
            struct BlockingContext {
                bool completed;
                WGPUMapAsyncStatus status;
            } blocking = {};
            callbackInfo.userdata1 = &blocking;
            callbackInfo.callback = [](WGPUMapAsyncStatus status, WGPUStringView, void* userdata1, void*) {
                BlockingContext& blocking = *(BlockingContext*)userdata1;
                blocking.completed = true;
                blocking.status = status;
            };
            WaitForFuture(m_Device.GetInstance(), wgpuBufferMapAsync(m_Buffer, WGPUMapMode_Read, mapOffset, mapSize, callbackInfo));
            if (blocking.status != WGPUMapAsyncStatus_Success) {
                m_MapOffset = 0;
                m_MapSize = 0;
                return nullptr;
            }
            m_MappedReadback = wgpuBufferGetConstMappedRange(m_Buffer, mapOffset, mapSize);
            if (!m_MappedReadback) {
                wgpuBufferUnmap(m_Buffer);
                m_MapOffset = 0;
                m_MapSize = 0;
                return nullptr;
            }
            return (uint8_t*)m_MappedReadback + (m_MapOffset - mapOffset);
        }
        if (!m_PendingMap) {
            m_PendingMap = new PendingMap;
            WGPUBufferMapCallbackInfo callbackInfo = WGPU_BUFFER_MAP_CALLBACK_INFO_INIT;
            callbackInfo.mode = WGPUCallbackMode_AllowSpontaneous;
            callbackInfo.userdata1 = m_PendingMap;
            callbackInfo.callback = [](WGPUMapAsyncStatus status, WGPUStringView, void* userdata1, void*) {
                PendingMap* pending = (PendingMap*)userdata1;
                if (pending->orphaned) {
                    delete pending;
                    return;
                }
                pending->completed = true;
                pending->status = status;
            };
            wgpuBufferMapAsync(m_Buffer, WGPUMapMode_Read, mapOffset, mapSize, callbackInfo);
            return nullptr;
        }
        if (!m_PendingMap->completed)
            return nullptr;

        struct MapContext {
            bool completed;
            WGPUMapAsyncStatus status;
        } context = { true, m_PendingMap->status };
        delete m_PendingMap;
        m_PendingMap = nullptr;
#else
        struct MapContext {
            bool completed;
            WGPUMapAsyncStatus status;
        } context = {};

        WGPUBufferMapCallbackInfo callbackInfo = WGPU_BUFFER_MAP_CALLBACK_INFO_INIT;
        callbackInfo.mode = WGPUCallbackMode_AllowProcessEvents;
        callbackInfo.userdata1 = &context;
        callbackInfo.callback = [](WGPUMapAsyncStatus status, WGPUStringView, void* userdata1, void*) {
            MapContext& context = *(MapContext*)userdata1;
            context.completed = true;
            context.status = status;
        };

        WGPUFuture mapFuture = wgpuBufferMapAsync(m_Buffer, WGPUMapMode_Read, mapOffset, mapSize, callbackInfo);
        MaybeUnused(mapFuture);

        // TODO: Readback map is blocking and pumps the device until completion.
        while (!context.completed) {
            wgpuDevicePoll(m_Device, WGPU_TRUE, nullptr);
            wgpuInstanceProcessEvents(m_Device.GetInstance());
        }
#endif

        if (context.status != WGPUMapAsyncStatus_Success) {
            m_MapOffset = 0;
            m_MapSize = 0;
            return nullptr;
        }

        m_MappedReadback = wgpuBufferGetConstMappedRange(m_Buffer, mapOffset, mapSize);
        if (!m_MappedReadback) {
            wgpuBufferUnmap(m_Buffer);
            m_MapOffset = 0;
            m_MapSize = 0;
            return nullptr;
        }

        return (uint8_t*)m_MappedReadback + (m_MapOffset - mapOffset);
    }

    if (m_CpuMemory.empty())
        m_CpuMemory.resize((size_t)Align(std::max<uint64_t>(m_Desc.size, 4), 4));

    // TODO: Host-visible upload buffers are CPU-shadowed and flushed through "wgpuQueueWriteBuffer" on unmap.
    return m_CpuMemory.data() + offset;
}

void BufferWGPU::FlushHostWrites(uint64_t offset, uint64_t size) {
    if (m_CpuMemory.empty() || m_MemoryLocation == MemoryLocation::DEVICE || m_MemoryLocation == MemoryLocation::HOST_READBACK || offset >= m_CpuMemory.size())
        return;

    uint64_t begin = offset & ~3ull;
    uint64_t end = std::min<uint64_t>(Align(offset + size, 4), m_CpuMemory.size());
    if (end > begin)
        m_Device.WriteBuffer(m_Buffer, begin, m_CpuMemory.data() + begin, (size_t)(end - begin));
}

void BufferWGPU::Unmap() {
    if (m_MemoryLocation == MemoryLocation::HOST_READBACK) {
        if (m_MappedReadback) {
            wgpuBufferUnmap(m_Buffer);
            m_MappedReadback = nullptr;
        }

        m_MapOffset = 0;
        m_MapSize = 0;
        return;
    }

    if (m_MapSize) {
        uint64_t writeOffset = m_MapOffset & ~3ull;
        uint64_t writeEnd = Align(m_MapOffset + m_MapSize, 4);

        if (writeEnd > m_MapOffset + m_MapSize)
            memset(m_CpuMemory.data() + m_MapOffset + m_MapSize, 0, (size_t)(writeEnd - (m_MapOffset + m_MapSize)));

        m_Device.WriteBuffer(m_Buffer, writeOffset, m_CpuMemory.data() + writeOffset, (size_t)(writeEnd - writeOffset));
    }

    m_MapOffset = 0;
    m_MapSize = 0;
}
