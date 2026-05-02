// © 2021 NVIDIA Corporation

#pragma once

#include "Extensions/NRIVideo.h"

#include <vulkan/vulkan.h>

#include <array>
#include <cstdint>

namespace nri {

inline uint8_t GetVideoEncodeAV1ReferenceNameIndexVK(VideoAV1ReferenceName name) {
    switch (name) {
    case VideoAV1ReferenceName::NONE:
        return STD_VIDEO_AV1_PRIMARY_REF_NONE;
    case VideoAV1ReferenceName::LAST:
        return 0;
    case VideoAV1ReferenceName::LAST2:
        return 1;
    case VideoAV1ReferenceName::LAST3:
        return 2;
    case VideoAV1ReferenceName::GOLDEN:
        return 3;
    case VideoAV1ReferenceName::BWDREF:
        return 4;
    case VideoAV1ReferenceName::ALTREF2:
        return 5;
    case VideoAV1ReferenceName::ALTREF:
        return 6;
    case VideoAV1ReferenceName::MAX_NUM:
        return STD_VIDEO_AV1_PRIMARY_REF_NONE;
    }

    return STD_VIDEO_AV1_PRIMARY_REF_NONE;
}

inline uint8_t GetVideoEncodeQPByFrameTypeVK(const VideoEncodeRateControlDesc& rateControlDesc, VideoEncodeFrameType frameType) {
    return frameType == VideoEncodeFrameType::B ? rateControlDesc.qpB : (frameType == VideoEncodeFrameType::P ? rateControlDesc.qpP : rateControlDesc.qpI);
}

struct VideoEncodeHEVCReferenceListsVK {
    std::array<uint32_t, STD_VIDEO_H265_MAX_NUM_LIST_REF> list0 = {};
    std::array<uint32_t, STD_VIDEO_H265_MAX_NUM_LIST_REF> list1 = {};
    uint32_t list0Num = 0;
    uint32_t list1Num = 0;
    uint32_t failingReference = 0;
    bool missingDescriptor = false;
    bool invalidPictureOrderCount = false;
};

inline const VideoH265ReferenceDesc* FindVideoH265ReferenceDescVK(const VideoH265ReferenceDesc* references, uint32_t referenceNum, uint32_t slot) {
    if (!references)
        return nullptr;

    for (uint32_t i = 0; i < referenceNum; i++) {
        if (references[i].slot == slot)
            return &references[i];
    }

    return nullptr;
}

inline bool BuildVideoEncodeHEVCReferenceListsVK(const VideoReference* references, const VideoH265ReferenceDesc* referenceDescs, uint32_t referenceNum,
    VideoEncodeFrameType frameType, int32_t currentPictureOrderCount, VideoEncodeHEVCReferenceListsVK& lists) {
    lists = {};

    if (referenceNum > STD_VIDEO_H265_MAX_NUM_LIST_REF) {
        lists.failingReference = STD_VIDEO_H265_MAX_NUM_LIST_REF;
        return false;
    }

    if (referenceNum && !referenceDescs) {
        lists.missingDescriptor = true;
        return false;
    }

    for (uint32_t i = 0; i < referenceNum; i++) {
        const VideoH265ReferenceDesc* referenceDesc = FindVideoH265ReferenceDescVK(referenceDescs, referenceNum, references[i].slot);
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

    if (referenceNum && !lists.list0Num) {
        lists.invalidPictureOrderCount = true;
        return false;
    }

    return true;
}

struct VideoEncodeAV1ReferenceMappingVK {
    std::array<int32_t, VK_MAX_VIDEO_AV1_REFERENCES_PER_FRAME_KHR> referenceNameSlotIndices = {};
    std::array<int8_t, VK_MAX_VIDEO_AV1_REFERENCES_PER_FRAME_KHR> refFrameIndices = {};
    uint32_t failingReference = 0;
    bool invalidName = false;
    bool missingResource = false;
    bool missingPrimaryReference = false;
};

inline bool HasVideoReferenceSlotVK(const VideoReference* references, uint32_t referenceNum, uint32_t slot) {
    for (uint32_t i = 0; i < referenceNum; i++) {
        if (references[i].slot == slot)
            return true;
    }

    return false;
}

inline bool BuildVideoEncodeAV1ReferenceMappingVK(const VideoReference* references, uint32_t referenceNum, const VideoAV1PictureDesc& pictureDesc,
    VideoEncodeAV1ReferenceMappingVK& mapping) {
    for (int32_t& slotIndex : mapping.referenceNameSlotIndices)
        slotIndex = -1;
    for (int8_t& refFrameIndex : mapping.refFrameIndices)
        refFrameIndex = -1;
    mapping.failingReference = 0;
    mapping.invalidName = false;
    mapping.missingResource = false;
    mapping.missingPrimaryReference = false;

    if (pictureDesc.referenceNum > 8) {
        mapping.failingReference = 8;
        return false;
    }

    for (uint32_t i = 0; i < pictureDesc.referenceNum; i++) {
        const VideoAV1ReferenceDesc& reference = pictureDesc.references[i];
        if (!HasVideoReferenceSlotVK(references, referenceNum, reference.slot)) {
            mapping.failingReference = i;
            mapping.missingResource = true;
            return false;
        }

        if (reference.refFrameIndex >= 8) {
            mapping.failingReference = i;
            return false;
        }

        const uint8_t referenceNameIndex = GetVideoEncodeAV1ReferenceNameIndexVK(reference.name);
        if (reference.name == VideoAV1ReferenceName::NONE)
            continue;

        if (referenceNameIndex >= VK_MAX_VIDEO_AV1_REFERENCES_PER_FRAME_KHR) {
            mapping.failingReference = i;
            mapping.invalidName = true;
            return false;
        }

        mapping.referenceNameSlotIndices[referenceNameIndex] = (int32_t)reference.slot;
        mapping.refFrameIndices[referenceNameIndex] = (int8_t)reference.refFrameIndex;
    }

    const uint8_t primaryReferenceIndex = GetVideoEncodeAV1ReferenceNameIndexVK(pictureDesc.primaryReferenceName);
    if (primaryReferenceIndex < VK_MAX_VIDEO_AV1_REFERENCES_PER_FRAME_KHR && mapping.referenceNameSlotIndices[primaryReferenceIndex] < 0) {
        mapping.missingPrimaryReference = true;
        return false;
    }

    return true;
}

inline bool IsVideoEncodeAV1KeyFrameReferenceStateValidVK(VideoEncodeFrameType frameType, uint32_t referenceNum) {
    return (frameType != VideoEncodeFrameType::IDR && frameType != VideoEncodeFrameType::I) || referenceNum == 0;
}

} // namespace nri
