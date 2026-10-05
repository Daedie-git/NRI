// © 2026 NVIDIA Corporation

#pragma once

namespace nri {

struct DescriptorRangeMappingWGPU {
    WGPUShaderStage visibility = WGPUShaderStage_None;
    uint32_t descriptorOffset = 0;
    uint32_t bindingBase = 0;
    uint32_t descriptorNum = 0;
    WGPUTextureSampleType textureSampleType = WGPUTextureSampleType_Float;
    WGPUTextureViewDimension textureViewDimension = WGPUTextureViewDimension_2D;
    WGPUBool textureMultisampled = WGPU_FALSE;
    WGPUTextureFormat storageTextureFormat = WGPUTextureFormat_Undefined;
    WGPUTextureViewDimension storageTextureViewDimension = WGPUTextureViewDimension_2D;
    WGPUStorageTextureAccess storageTextureAccess = WGPUStorageTextureAccess_WriteOnly;
    DescriptorType type = DescriptorType::TEXTURE;
    bool isArray = false;
};

struct DescriptorSetMappingWGPU {
    inline DescriptorSetMappingWGPU(const StdAllocator<uint8_t>& allocator)
        : ranges(allocator) {
    }

    Vector<DescriptorRangeMappingWGPU> ranges;
    WGPUBindGroupLayout layout = nullptr;
    uint32_t bindGroupIndex = 0;
    uint32_t layoutVersion = 1;
};

struct DescriptorSetBindGroupWGPU {
    WGPUBindGroupLayout layout = nullptr;
    WGPUBindGroup bindGroup = nullptr;
    uint64_t updateVersion = 0;
};

struct DescriptorSetWGPU final : public DebugNameBase {
    DescriptorSetWGPU(DeviceWGPU& device, const DescriptorSetMappingWGPU& mapping, bool isCopySource);
    ~DescriptorSetWGPU();

    inline WGPUBindGroup GetBindGroup() {
        return GetBindGroup(m_Mapping);
    }

    inline DeviceWGPU& GetDevice() const {
        return m_Device;
    }

    inline uint64_t GetUpdateVersion() const {
        return m_UpdateVersion;
    }

    void UpdateRange(uint32_t rangeIndex, uint32_t baseDescriptor, const Descriptor* const* descriptors, uint32_t descriptorNum);
    void CopyRangeFrom(uint32_t dstRangeIndex, uint32_t dstBaseDescriptor, const DescriptorSetWGPU& srcDescriptorSet, uint32_t srcRangeIndex, uint32_t srcBaseDescriptor, uint32_t descriptorNum);
    void FinalizeUpdate();
    void GetOffsets(uint32_t& resourceHeapOffset, uint32_t& samplerHeapOffset) const;
    WGPUBindGroup GetBindGroup(const DescriptorSetMappingWGPU& mapping);

private:
    bool RecreateBindGroup(const DescriptorSetMappingWGPU& mapping, DescriptorSetBindGroupWGPU& cache);

private:
    DeviceWGPU& m_Device;
    const DescriptorSetMappingWGPU& m_Mapping;
    Vector<DescriptorWGPU*> m_Descriptors;
    Vector<DescriptorSetBindGroupWGPU> m_BindGroups;
    uint64_t m_UpdateVersion = 1;
    bool m_IsCopySource = false;
    Lock m_BindGroupLock;
};

} // namespace nri
