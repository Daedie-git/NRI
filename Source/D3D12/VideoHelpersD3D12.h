// © 2021 NVIDIA Corporation

#pragma once

#include "Extensions/NRIVideo.h"

#include <d3d12video.h>
#include <dxva.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>

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

inline const VideoH264SequenceParameterSetDesc* FindVideoH264SequenceParameterSetD3D12(const VideoH264SessionParametersDesc& parameters, uint8_t id) {
    if (!parameters.sequenceParameterSets)
        return nullptr;

    for (uint32_t i = 0; i < parameters.sequenceParameterSetNum; i++) {
        if (parameters.sequenceParameterSets[i].sequenceParameterSetId == id)
            return &parameters.sequenceParameterSets[i];
    }

    return nullptr;
}

inline const VideoH264PictureParameterSetDesc* FindVideoH264PictureParameterSetD3D12(const VideoH264SessionParametersDesc& parameters, uint8_t id) {
    if (!parameters.pictureParameterSets)
        return nullptr;

    for (uint32_t i = 0; i < parameters.pictureParameterSetNum; i++) {
        if (parameters.pictureParameterSets[i].pictureParameterSetId == id)
            return &parameters.pictureParameterSets[i];
    }

    return nullptr;
}

inline bool CanBuildVideoDecodeH264ArgumentsD3D12(const VideoDecodeDesc& desc) {
    return desc.h264PictureDesc && !desc.h265PictureDesc && desc.argumentNum == 0 &&
        (desc.referenceNum == 0 || (desc.h264PictureDesc->references && desc.h264PictureDesc->referenceNum == desc.referenceNum));
}

inline const VideoH264ReferenceDesc* FindVideoH264ReferenceDescD3D12(const VideoH264ReferenceDesc* references, uint32_t referenceNum, uint32_t slot) {
    if (!references)
        return nullptr;

    for (uint32_t i = 0; i < referenceNum; i++) {
        if (references[i].slot == slot)
            return &references[i];
    }

    return nullptr;
}

inline bool BuildVideoDecodeH264ArgumentsD3D12(const VideoH264SessionParametersDesc& parameters, const VideoH264DecodePictureDesc& pictureDesc, uint64_t bitstreamSize,
    uint32_t dstSlot, DXVA_PicParams_H264& pictureParameters, DXVA_Qmatrix_H264& inverseQuantizationMatrix, DXVA_Slice_H264_Short* slices, uint32_t sliceNum) {
    if (sliceNum == 0 || sliceNum != pictureDesc.sliceOffsetNum || !pictureDesc.sliceOffsets || !slices)
        return false;

    if (pictureDesc.referenceNum > 16 || (pictureDesc.referenceNum && !pictureDesc.references))
        return false;

    uint16_t usedReferenceSlots = 0;
    for (uint32_t i = 0; i < pictureDesc.referenceNum; i++) {
        const VideoH264ReferenceDesc& reference = pictureDesc.references[i];
        if (reference.slot >= 16 || (usedReferenceSlots & (1u << reference.slot)))
            return false;
        usedReferenceSlots |= 1u << reference.slot;
    }

    const VideoH264PictureParameterSetDesc* pps = FindVideoH264PictureParameterSetD3D12(parameters, pictureDesc.pictureParameterSetId);
    if (!pps)
        return false;

    const VideoH264SequenceParameterSetDesc* sps = FindVideoH264SequenceParameterSetD3D12(parameters, pictureDesc.sequenceParameterSetId);
    if (!sps || pps->sequenceParameterSetId != sps->sequenceParameterSetId)
        return false;

    if (bitstreamSize > UINT32_MAX)
        return false;

    pictureParameters = {};
    pictureParameters.wFrameWidthInMbsMinus1 = sps->pictureWidthInMbsMinus1;
    pictureParameters.wFrameHeightInMbsMinus1 = sps->pictureHeightInMapUnitsMinus1;
    if (dstSlot > 0x7F)
        return false;

    pictureParameters.CurrPic.bPicEntry = (uint8_t)dstSlot;
    pictureParameters.CurrPic.AssociatedFlag = !!(pictureDesc.flags & VideoH264DecodePictureBits::BOTTOM_FIELD);
    pictureParameters.num_ref_frames = sps->referenceFrameNum;
    pictureParameters.field_pic_flag = !!(pictureDesc.flags & VideoH264DecodePictureBits::FIELD_PICTURE);
    pictureParameters.MbaffFrameFlag = !!(sps->flags & VideoH264SequenceParameterSetBits::MB_ADAPTIVE_FRAME_FIELD) && !pictureParameters.field_pic_flag;
    pictureParameters.chroma_format_idc = sps->chromaFormatIdc;
    pictureParameters.RefPicFlag = !!(pictureDesc.flags & VideoH264DecodePictureBits::REFERENCE);
    pictureParameters.constrained_intra_pred_flag = !!(pps->flags & VideoH264PictureParameterSetBits::CONSTRAINED_INTRA_PRED);
    pictureParameters.weighted_pred_flag = !!(pps->flags & VideoH264PictureParameterSetBits::WEIGHTED_PRED);
    pictureParameters.weighted_bipred_idc = pps->weightedBipredIdc;
    pictureParameters.MbsConsecutiveFlag = 1;
    pictureParameters.frame_mbs_only_flag = !!(sps->flags & VideoH264SequenceParameterSetBits::FRAME_MBS_ONLY);
    pictureParameters.transform_8x8_mode_flag = !!(pps->flags & VideoH264PictureParameterSetBits::TRANSFORM_8X8_MODE);
    pictureParameters.MinLumaBipredSize8x8Flag = sps->levelIdc >= 31;
    pictureParameters.IntraPicFlag = !!(pictureDesc.flags & VideoH264DecodePictureBits::INTRA);
    pictureParameters.bit_depth_luma_minus8 = sps->bitDepthLumaMinus8;
    pictureParameters.bit_depth_chroma_minus8 = sps->bitDepthChromaMinus8;
    pictureParameters.Reserved16Bits = 3;
    pictureParameters.StatusReportFeedbackNumber = 1;
    for (uint32_t i = 0; i < 16; i++)
        pictureParameters.RefFrameList[i].bPicEntry = 0xff;
    for (uint32_t i = 0; i < pictureDesc.referenceNum; i++) {
        const VideoH264ReferenceDesc& reference = pictureDesc.references[i];
        pictureParameters.RefFrameList[reference.slot].Index7Bits = (UCHAR)reference.slot;
        pictureParameters.RefFrameList[reference.slot].AssociatedFlag = reference.longTermReference != 0;
        pictureParameters.FieldOrderCntList[reference.slot][0] = reference.pictureOrderCount;
        pictureParameters.FieldOrderCntList[reference.slot][1] = reference.pictureOrderCount;
        pictureParameters.FrameNumList[reference.slot] = (USHORT)reference.frameNum;
        pictureParameters.UsedForReferenceFlags |= 3u << (reference.slot * 2);
    }
    pictureParameters.CurrFieldOrderCnt[0] = pictureDesc.topFieldOrderCount;
    pictureParameters.CurrFieldOrderCnt[1] = pictureDesc.bottomFieldOrderCount;
    pictureParameters.pic_init_qs_minus26 = pps->pictureInitQsMinus26;
    pictureParameters.chroma_qp_index_offset = pps->chromaQpIndexOffset;
    pictureParameters.second_chroma_qp_index_offset = pps->secondChromaQpIndexOffset;
    pictureParameters.ContinuationFlag = 1;
    pictureParameters.pic_init_qp_minus26 = pps->pictureInitQpMinus26;
    pictureParameters.num_ref_idx_l0_active_minus1 = pps->refIndexL0DefaultActiveMinus1;
    pictureParameters.num_ref_idx_l1_active_minus1 = pps->refIndexL1DefaultActiveMinus1;
    pictureParameters.frame_num = pictureDesc.frameNum;
    pictureParameters.log2_max_frame_num_minus4 = sps->log2MaxFrameNumMinus4;
    pictureParameters.pic_order_cnt_type = sps->pictureOrderCountType;
    pictureParameters.log2_max_pic_order_cnt_lsb_minus4 = sps->log2MaxPictureOrderCountLsbMinus4;
    pictureParameters.delta_pic_order_always_zero_flag = !!(sps->flags & VideoH264SequenceParameterSetBits::DELTA_PIC_ORDER_ALWAYS_ZERO);
    pictureParameters.direct_8x8_inference_flag = !!(sps->flags & VideoH264SequenceParameterSetBits::DIRECT_8X8_INFERENCE);
    pictureParameters.entropy_coding_mode_flag = !!(pps->flags & VideoH264PictureParameterSetBits::ENTROPY_CODING_MODE);
    pictureParameters.pic_order_present_flag = !!(pps->flags & VideoH264PictureParameterSetBits::BOTTOM_FIELD_PIC_ORDER_IN_FRAME);
    pictureParameters.deblocking_filter_control_present_flag = !!(pps->flags & VideoH264PictureParameterSetBits::DEBLOCKING_FILTER_CONTROL_PRESENT);
    pictureParameters.redundant_pic_cnt_present_flag = !!(pps->flags & VideoH264PictureParameterSetBits::REDUNDANT_PIC_CNT_PRESENT);

    inverseQuantizationMatrix = {};
    std::memset(inverseQuantizationMatrix.bScalingLists4x4, 16, sizeof(inverseQuantizationMatrix.bScalingLists4x4));
    std::memset(inverseQuantizationMatrix.bScalingLists8x8, 16, sizeof(inverseQuantizationMatrix.bScalingLists8x8));

    for (uint32_t i = 0; i < sliceNum; i++) {
        const uint32_t offset = pictureDesc.sliceOffsets[i];
        if (offset >= bitstreamSize || (i + 1 < sliceNum && pictureDesc.sliceOffsets[i + 1] <= offset))
            return false;

        const uint64_t nextOffset = i + 1 < sliceNum ? pictureDesc.sliceOffsets[i + 1] : bitstreamSize;
        const uint64_t size = nextOffset - offset;
        if (size > UINT32_MAX)
            return false;

        slices[i] = {};
        slices[i].BSNALunitDataLocation = offset;
        slices[i].SliceBytesInBuffer = (UINT)size;
    }

    return true;
}

inline bool IsVideoEncodeFrameTypeSupportedByD3D12NoBGop(VideoCodec codec, VideoEncodeFrameType frameType) {
    return frameType != VideoEncodeFrameType::B || (codec != VideoCodec::H264 && codec != VideoCodec::H265);
}

inline bool IsVideoEncodePictureUsedAsReferenceD3D12(VideoCodec codec, uint32_t maxReferenceNum, bool hasReconstructedPicture, uint8_t av1RefreshFrameFlags) {
    if (!maxReferenceNum || !hasReconstructedPicture)
        return false;

    return codec != VideoCodec::AV1 || av1RefreshFrameFlags != 0;
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

        if (referenceDesc->listIndex == 0) {
            if (referenceDesc->pictureOrderCount >= currentPictureOrderCount) {
                lists.failingReference = i;
                lists.invalidPictureOrderCount = true;
                return false;
            }

            lists.list0[lists.list0Num++] = i;
        } else if (referenceDesc->listIndex == 1) {
            if (frameType != VideoEncodeFrameType::B) {
                lists.failingReference = i;
                lists.invalidPictureOrderCount = true;
                return false;
            }
            if (referenceDesc->pictureOrderCount <= currentPictureOrderCount) {
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
