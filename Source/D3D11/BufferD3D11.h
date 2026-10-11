// © 2021 NVIDIA Corporation

#pragma once

namespace nri {

struct QueryRange {
    const QueryPoolD3D11* pool;
    uint64_t bufferOffset;
    uint32_t offset;
    uint32_t num;
};

struct TextureReadbackD3D11 {
    TextureD3D11* texture;
    TextureDataLayoutDesc dataLayout;
    uint32_t rowSize;
    uint32_t rowNum;
};

struct BufferD3D11 final : public DebugNameBase {
    inline BufferD3D11(DeviceD3D11& device)
        : m_Device(device)
        , m_TextureReadbacks(device.GetStdAllocator()) {
    }

    inline operator ID3D11Buffer*() const {
        return m_Buffer;
    }

    inline const BufferDesc& GetDesc() const {
        return m_Desc;
    }

    inline DeviceD3D11& GetDevice() const {
        return m_Device;
    }

    inline void AssignQueryPoolRange(const QueryPoolD3D11* queryPool, uint32_t offset, uint32_t num, uint64_t bufferOffset) {
        m_QueryRange.pool = queryPool;
        m_QueryRange.offset = offset;
        m_QueryRange.num = num;
        m_QueryRange.bufferOffset = bufferOffset;
    }

    ~BufferD3D11();

    Result Create(const BufferDesc& bufferDesc);
    Result Create(const BufferD3D11Desc& bufferD3D11Desc);
    Result Allocate(MemoryLocation memoryLocation, float priority);
    void AddTextureReadback(TextureReadbackD3D11& textureReadback);

    //================================================================================================================
    // DebugNameBase
    //================================================================================================================

    void SetDebugName(const char* name) NRI_DEBUG_NAME_OVERRIDE {
        NRI_SET_D3D_DEBUG_OBJECT_NAME(m_Buffer, name);
    }

    //================================================================================================================
    // NRI
    //================================================================================================================

    void* Map(uint64_t offset);
    void Unmap();

private:
    DeviceD3D11& m_Device;
    ComPtr<ID3D11Buffer> m_Buffer;
    Vector<TextureReadbackD3D11> m_TextureReadbacks;
    BufferDesc m_Desc = {};
    QueryRange m_QueryRange = {};
};

} // namespace nri
