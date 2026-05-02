// © 2021 NVIDIA Corporation

#pragma once

#include "Extensions/NRIVideo.h"

#include <d3d12video.h>

#include <algorithm>
#include <array>
#include <cstdint>

namespace nri {

constexpr uint32_t VIDEO_D3D12_DECODE_MAX_PIC_ENTRY_SLOT = 127;
constexpr uint32_t VIDEO_D3D12_HEVC_MAX_REFERENCE_NUM = 15;
constexpr uint32_t VIDEO_D3D12_AV1_FEATURE_FLAG_128x128_SUPERBLOCK = 0x1;
constexpr uint32_t VIDEO_D3D12_AV1_FEATURE_FLAG_SUPER_RESOLUTION = 0x200;
constexpr uint32_t VIDEO_D3D12_AV1_FEATURE_FLAG_LOOP_RESTORATION_FILTER = 0x400;
constexpr uint32_t VIDEO_D3D12_AV1_FEATURE_FLAG_CDEF_FILTERING = 0x1000;
constexpr uint32_t VIDEO_D3D12_AV1_FEATURE_FLAG_ORDER_HINT_TOOLS = 0x8000;
constexpr uint32_t VIDEO_D3D12_AV1_FEATURE_FLAG_AUTO_SEGMENTATION = 0x10000;
constexpr uint32_t VIDEO_D3D12_AV1_FEATURE_FLAG_LOOP_FILTER_DELTAS = 0x40000;
constexpr uint32_t VIDEO_D3D12_AV1_FEATURE_FLAG_QUANTIZATION_DELTAS = 0x80000;
constexpr uint32_t VIDEO_D3D12_AV1_FEATURE_FLAG_FORCED_INTEGER_MOTION_VECTORS = 0x100;

struct VideoDecodeReferenceLayoutD3D12 {
    uint32_t slotCount = 0;
    uint32_t failingReference = 0;
    bool duplicateSlot = false;
};

inline bool GetVideoDecodeReferenceLayoutD3D12(const VideoReference* references, uint32_t referenceNum, VideoDecodeReferenceLayoutD3D12& layout) {
    layout = {};
    std::array<bool, VIDEO_D3D12_DECODE_MAX_PIC_ENTRY_SLOT + 1> usedSlots = {};

    for (uint32_t i = 0; i < referenceNum; i++) {
        const uint32_t slot = references[i].slot;
        if (slot > VIDEO_D3D12_DECODE_MAX_PIC_ENTRY_SLOT) {
            layout.failingReference = i;
            return false;
        }

        if (usedSlots[slot]) {
            layout.failingReference = i;
            layout.duplicateSlot = true;
            return false;
        }

        usedSlots[slot] = true;
        layout.slotCount = std::max(layout.slotCount, slot + 1);
    }

    return true;
}

inline bool HasVideoDecodeNeutralCodecDescriptorsD3D12(const VideoDecodeDesc& desc) {
    return desc.h264PictureDesc || desc.h265PictureDesc;
}

inline bool IsVideoEncodeFrameTypeSupportedByD3D12NoBGop(VideoCodec codec, VideoEncodeFrameType frameType) {
    return frameType != VideoEncodeFrameType::B || (codec != VideoCodec::H264 && codec != VideoCodec::H265);
}

struct VideoEncodeHEVCReferenceListsD3D12 {
    std::array<uint32_t, VIDEO_D3D12_HEVC_MAX_REFERENCE_NUM> list0 = {};
    std::array<uint32_t, VIDEO_D3D12_HEVC_MAX_REFERENCE_NUM> list1 = {};
    uint32_t list0Num = 0;
    uint32_t list1Num = 0;
    uint32_t failingReference = 0;
    bool missingDescriptor = false;
    bool invalidPictureOrderCount = false;
};

inline const VideoH265ReferenceDesc* FindVideoH265ReferenceDescD3D12(const VideoH265ReferenceDesc* references, uint32_t referenceNum, uint32_t slot) {
    if (!references)
        return nullptr;

    for (uint32_t i = 0; i < referenceNum; i++) {
        if (references[i].slot == slot)
            return &references[i];
    }

    return nullptr;
}

inline bool BuildVideoEncodeHEVCReferenceListsD3D12(const VideoReference* references, const VideoH265ReferenceDesc* referenceDescs, uint32_t referenceNum,
    VideoEncodeFrameType frameType, int32_t currentPictureOrderCount, VideoEncodeHEVCReferenceListsD3D12& lists) {
    lists = {};

    if (referenceNum > VIDEO_D3D12_HEVC_MAX_REFERENCE_NUM) {
        lists.failingReference = VIDEO_D3D12_HEVC_MAX_REFERENCE_NUM;
        return false;
    }

    if (referenceNum && !referenceDescs) {
        lists.missingDescriptor = true;
        return false;
    }

    for (uint32_t i = 0; i < referenceNum; i++) {
        const VideoH265ReferenceDesc* referenceDesc = FindVideoH265ReferenceDescD3D12(referenceDescs, referenceNum, references[i].slot);
        if (!referenceDesc) {
            lists.failingReference = i;
            lists.missingDescriptor = true;
            return false;
        }

        if (referenceDesc->pictureOrderCount < currentPictureOrderCount)
            lists.list0[lists.list0Num++] = i;
        else if (referenceDesc->pictureOrderCount > currentPictureOrderCount) {
            if (frameType != VideoEncodeFrameType::B) {
                lists.failingReference = i;
                lists.invalidPictureOrderCount = true;
                return false;
            }

            lists.list1[lists.list1Num++] = i;
        } else {
            lists.failingReference = i;
            lists.invalidPictureOrderCount = true;
            return false;
        }
    }

    return true;
}

inline bool IsVideoEncodeAV1RequiredFeatureSetSupportedD3D12(uint32_t featureFlags) {
    constexpr uint32_t supportedFeatureFlags =
        VIDEO_D3D12_AV1_FEATURE_FLAG_ORDER_HINT_TOOLS |
        VIDEO_D3D12_AV1_FEATURE_FLAG_LOOP_RESTORATION_FILTER |
        VIDEO_D3D12_AV1_FEATURE_FLAG_FORCED_INTEGER_MOTION_VECTORS |
        VIDEO_D3D12_AV1_FEATURE_FLAG_AUTO_SEGMENTATION |
        VIDEO_D3D12_AV1_FEATURE_FLAG_CDEF_FILTERING |
        VIDEO_D3D12_AV1_FEATURE_FLAG_QUANTIZATION_DELTAS |
        VIDEO_D3D12_AV1_FEATURE_FLAG_LOOP_FILTER_DELTAS;

    return (featureFlags & ~supportedFeatureFlags) == 0;
}

} // namespace nri
