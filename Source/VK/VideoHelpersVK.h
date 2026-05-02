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

inline bool IsVideoEncodePictureUsedAsReferenceVK(VideoCodec codec, uint32_t maxReferenceNum, bool hasReconstructedPicture, uint8_t av1RefreshFrameFlags) {
    if (!maxReferenceNum || !hasReconstructedPicture)
        return false;

    return codec != VideoCodec::AV1 || av1RefreshFrameFlags != 0;
}

inline StdVideoAV1Level GetVideoAV1LevelVK(uint32_t width, uint32_t height) {
    const uint64_t samples = uint64_t(width) * height;
    if (samples <= 512ull * 288ull)
        return STD_VIDEO_AV1_LEVEL_2_0;
    if (samples <= 704ull * 396ull)
        return STD_VIDEO_AV1_LEVEL_2_1;
    if (samples <= 1088ull * 612ull)
        return STD_VIDEO_AV1_LEVEL_3_0;
    if (samples <= 1376ull * 774ull)
        return STD_VIDEO_AV1_LEVEL_3_1;
    if (samples <= 2048ull * 1152ull)
        return STD_VIDEO_AV1_LEVEL_4_0;
    if (samples <= 4096ull * 2176ull)
        return STD_VIDEO_AV1_LEVEL_5_0;

    return STD_VIDEO_AV1_LEVEL_5_1;
}

inline StdVideoAV1Level GetVideoAV1LevelVK(uint8_t level, uint32_t width, uint32_t height) {
    switch (level) {
    case 20:
        return STD_VIDEO_AV1_LEVEL_2_0;
    case 21:
        return STD_VIDEO_AV1_LEVEL_2_1;
    case 30:
        return STD_VIDEO_AV1_LEVEL_3_0;
    case 31:
        return STD_VIDEO_AV1_LEVEL_3_1;
    case 40:
        return STD_VIDEO_AV1_LEVEL_4_0;
    case 41:
        return STD_VIDEO_AV1_LEVEL_4_1;
    case 50:
        return STD_VIDEO_AV1_LEVEL_5_0;
    case 51:
        return STD_VIDEO_AV1_LEVEL_5_1;
    case 52:
        return STD_VIDEO_AV1_LEVEL_5_2;
    case 53:
        return STD_VIDEO_AV1_LEVEL_5_3;
    case 60:
        return STD_VIDEO_AV1_LEVEL_6_0;
    case 61:
        return STD_VIDEO_AV1_LEVEL_6_1;
    case 62:
        return STD_VIDEO_AV1_LEVEL_6_2;
    case 63:
        return STD_VIDEO_AV1_LEVEL_6_3;
    case 70:
        return STD_VIDEO_AV1_LEVEL_7_0;
    case 71:
        return STD_VIDEO_AV1_LEVEL_7_1;
    case 72:
        return STD_VIDEO_AV1_LEVEL_7_2;
    case 73:
        return STD_VIDEO_AV1_LEVEL_7_3;
    default:
        return GetVideoAV1LevelVK(width, height);
    }
}

inline uint8_t GetVideoAV1SizeBitsMinus1VK(uint32_t value) {
    uint8_t bits = 0;
    value--;
    do {
        bits++;
        value >>= 1;
    } while (value);

    return bits - 1;
}

inline VideoAV1SequenceDesc GetDefaultVideoAV1SequenceDescVK(uint32_t width, uint32_t height, Format format) {
    VideoAV1SequenceDesc desc = {};
    desc.flags = VideoAV1SequenceBits::ENABLE_ORDER_HINT |
        VideoAV1SequenceBits::COLOR_DESCRIPTION_PRESENT;
    desc.bitDepth = format == Format::P010_UNORM || format == Format::P016_UNORM ? 10 : 8;
    desc.subsamplingX = 1;
    desc.subsamplingY = 1;
    desc.maxFrameWidthMinus1 = (uint16_t)(width - 1);
    desc.maxFrameHeightMinus1 = (uint16_t)(height - 1);
    desc.frameWidthBitsMinus1 = GetVideoAV1SizeBitsMinus1VK(width);
    desc.frameHeightBitsMinus1 = GetVideoAV1SizeBitsMinus1VK(height);
    desc.orderHintBitsMinus1 = 7;
    desc.seqForceIntegerMv = STD_VIDEO_AV1_SELECT_INTEGER_MV;
    desc.seqForceScreenContentTools = STD_VIDEO_AV1_SELECT_SCREEN_CONTENT_TOOLS;
    desc.level = 0;
    desc.colorPrimaries = STD_VIDEO_AV1_COLOR_PRIMARIES_BT_709;
    desc.transferCharacteristics = STD_VIDEO_AV1_TRANSFER_CHARACTERISTICS_BT_709;
    desc.matrixCoefficients = STD_VIDEO_AV1_MATRIX_COEFFICIENTS_BT_709;
    desc.chromaSamplePosition = STD_VIDEO_AV1_CHROMA_SAMPLE_POSITION_VERTICAL;
    return desc;
}

inline VideoAV1PictureBits GetDefaultVideoAV1PictureFlagsVK() {
    return VideoAV1PictureBits::ERROR_RESILIENT_MODE |
        VideoAV1PictureBits::DISABLE_CDF_UPDATE |
        VideoAV1PictureBits::ALLOW_SCREEN_CONTENT_TOOLS |
        VideoAV1PictureBits::FORCE_INTEGER_MV |
        VideoAV1PictureBits::SHOW_FRAME |
        VideoAV1PictureBits::SHOWABLE_FRAME;
}

inline void FillVideoAV1ColorConfigVK(StdVideoAV1ColorConfig& colorConfig, const VideoAV1SequenceDesc& desc) {
    colorConfig = {};
    colorConfig.flags.mono_chrome = !!(desc.flags & VideoAV1SequenceBits::MONO_CHROME);
    colorConfig.flags.color_range = !!(desc.flags & VideoAV1SequenceBits::COLOR_RANGE);
    colorConfig.flags.separate_uv_delta_q = !!(desc.flags & VideoAV1SequenceBits::SEPARATE_UV_DELTA_Q);
    colorConfig.flags.color_description_present_flag = !!(desc.flags & VideoAV1SequenceBits::COLOR_DESCRIPTION_PRESENT);
    colorConfig.BitDepth = desc.bitDepth;
    colorConfig.subsampling_x = desc.subsamplingX;
    colorConfig.subsampling_y = desc.subsamplingY;
    colorConfig.color_primaries = (StdVideoAV1ColorPrimaries)desc.colorPrimaries;
    colorConfig.transfer_characteristics = (StdVideoAV1TransferCharacteristics)desc.transferCharacteristics;
    colorConfig.matrix_coefficients = (StdVideoAV1MatrixCoefficients)desc.matrixCoefficients;
    colorConfig.chroma_sample_position = (StdVideoAV1ChromaSamplePosition)desc.chromaSamplePosition;
}

inline void FillVideoAV1SequenceHeaderVK(StdVideoAV1SequenceHeader& sequenceHeader, const VideoAV1SequenceDesc& desc, const StdVideoAV1ColorConfig& colorConfig,
    const StdVideoAV1TimingInfo* timingInfo) {
    sequenceHeader = {};
    sequenceHeader.flags.still_picture = !!(desc.flags & VideoAV1SequenceBits::STILL_PICTURE);
    sequenceHeader.flags.reduced_still_picture_header = !!(desc.flags & VideoAV1SequenceBits::REDUCED_STILL_PICTURE_HEADER);
    sequenceHeader.flags.use_128x128_superblock = !!(desc.flags & VideoAV1SequenceBits::USE_128X128_SUPERBLOCK);
    sequenceHeader.flags.enable_filter_intra = !!(desc.flags & VideoAV1SequenceBits::ENABLE_FILTER_INTRA);
    sequenceHeader.flags.enable_intra_edge_filter = !!(desc.flags & VideoAV1SequenceBits::ENABLE_INTRA_EDGE_FILTER);
    sequenceHeader.flags.enable_interintra_compound = !!(desc.flags & VideoAV1SequenceBits::ENABLE_INTERINTRA_COMPOUND);
    sequenceHeader.flags.enable_masked_compound = !!(desc.flags & VideoAV1SequenceBits::ENABLE_MASKED_COMPOUND);
    sequenceHeader.flags.enable_warped_motion = !!(desc.flags & VideoAV1SequenceBits::ENABLE_WARPED_MOTION);
    sequenceHeader.flags.enable_dual_filter = !!(desc.flags & VideoAV1SequenceBits::ENABLE_DUAL_FILTER);
    sequenceHeader.flags.enable_order_hint = !!(desc.flags & VideoAV1SequenceBits::ENABLE_ORDER_HINT);
    sequenceHeader.flags.enable_jnt_comp = !!(desc.flags & VideoAV1SequenceBits::ENABLE_JNT_COMP);
    sequenceHeader.flags.enable_ref_frame_mvs = !!(desc.flags & VideoAV1SequenceBits::ENABLE_REF_FRAME_MVS);
    sequenceHeader.flags.frame_id_numbers_present_flag = !!(desc.flags & VideoAV1SequenceBits::FRAME_ID_NUMBERS_PRESENT);
    sequenceHeader.flags.enable_superres = !!(desc.flags & VideoAV1SequenceBits::ENABLE_SUPERRES);
    sequenceHeader.flags.enable_cdef = !!(desc.flags & VideoAV1SequenceBits::ENABLE_CDEF);
    sequenceHeader.flags.enable_restoration = !!(desc.flags & VideoAV1SequenceBits::ENABLE_RESTORATION);
    sequenceHeader.flags.film_grain_params_present = !!(desc.flags & VideoAV1SequenceBits::FILM_GRAIN_PARAMS_PRESENT);
    sequenceHeader.flags.timing_info_present_flag = timingInfo != nullptr;
    sequenceHeader.flags.initial_display_delay_present_flag = !!(desc.flags & VideoAV1SequenceBits::INITIAL_DISPLAY_DELAY_PRESENT);
    sequenceHeader.seq_profile = (StdVideoAV1Profile)desc.seqProfile;
    sequenceHeader.frame_width_bits_minus_1 = desc.frameWidthBitsMinus1;
    sequenceHeader.frame_height_bits_minus_1 = desc.frameHeightBitsMinus1;
    sequenceHeader.max_frame_width_minus_1 = desc.maxFrameWidthMinus1;
    sequenceHeader.max_frame_height_minus_1 = desc.maxFrameHeightMinus1;
    sequenceHeader.delta_frame_id_length_minus_2 = desc.deltaFrameIdLengthMinus2;
    sequenceHeader.additional_frame_id_length_minus_1 = desc.additionalFrameIdLengthMinus1;
    sequenceHeader.order_hint_bits_minus_1 = desc.orderHintBitsMinus1;
    sequenceHeader.seq_force_integer_mv = desc.seqForceIntegerMv;
    sequenceHeader.seq_force_screen_content_tools = desc.seqForceScreenContentTools;
    sequenceHeader.pColorConfig = &colorConfig;
    sequenceHeader.pTimingInfo = timingInfo;
}

inline void FillVideoAV1PictureFlagsVK(StdVideoDecodeAV1PictureInfoFlags& flags, VideoAV1PictureBits bits) {
    flags.error_resilient_mode = !!(bits & VideoAV1PictureBits::ERROR_RESILIENT_MODE);
    flags.disable_cdf_update = !!(bits & VideoAV1PictureBits::DISABLE_CDF_UPDATE);
    flags.use_superres = !!(bits & VideoAV1PictureBits::USE_SUPERRES);
    flags.render_and_frame_size_different = !!(bits & VideoAV1PictureBits::RENDER_AND_FRAME_SIZE_DIFFERENT);
    flags.allow_screen_content_tools = !!(bits & VideoAV1PictureBits::ALLOW_SCREEN_CONTENT_TOOLS);
    flags.is_filter_switchable = !!(bits & VideoAV1PictureBits::IS_FILTER_SWITCHABLE);
    flags.force_integer_mv = !!(bits & VideoAV1PictureBits::FORCE_INTEGER_MV);
    flags.frame_size_override_flag = !!(bits & VideoAV1PictureBits::FRAME_SIZE_OVERRIDE);
    flags.buffer_removal_time_present_flag = !!(bits & VideoAV1PictureBits::BUFFER_REMOVAL_TIME_PRESENT);
    flags.allow_intrabc = !!(bits & VideoAV1PictureBits::ALLOW_INTRABC);
    flags.frame_refs_short_signaling = !!(bits & VideoAV1PictureBits::FRAME_REFS_SHORT_SIGNALING);
    flags.allow_high_precision_mv = !!(bits & VideoAV1PictureBits::ALLOW_HIGH_PRECISION_MV);
    flags.is_motion_mode_switchable = !!(bits & VideoAV1PictureBits::IS_MOTION_MODE_SWITCHABLE);
    flags.use_ref_frame_mvs = !!(bits & VideoAV1PictureBits::USE_REF_FRAME_MVS);
    flags.disable_frame_end_update_cdf = !!(bits & VideoAV1PictureBits::DISABLE_FRAME_END_UPDATE_CDF);
    flags.allow_warped_motion = !!(bits & VideoAV1PictureBits::ALLOW_WARPED_MOTION);
    flags.reduced_tx_set = !!(bits & VideoAV1PictureBits::REDUCED_TX_SET);
    flags.reference_select = !!(bits & VideoAV1PictureBits::REFERENCE_SELECT);
    flags.skip_mode_present = !!(bits & VideoAV1PictureBits::SKIP_MODE_PRESENT);
    flags.delta_q_present = !!(bits & VideoAV1PictureBits::DELTA_Q_PRESENT);
    flags.delta_lf_present = !!(bits & VideoAV1PictureBits::DELTA_LF_PRESENT);
    flags.delta_lf_multi = !!(bits & VideoAV1PictureBits::DELTA_LF_MULTI);
    flags.segmentation_enabled = !!(bits & VideoAV1PictureBits::SEGMENTATION_ENABLED);
    flags.segmentation_update_map = !!(bits & VideoAV1PictureBits::SEGMENTATION_UPDATE_MAP);
    flags.segmentation_temporal_update = !!(bits & VideoAV1PictureBits::SEGMENTATION_TEMPORAL_UPDATE);
    flags.segmentation_update_data = !!(bits & VideoAV1PictureBits::SEGMENTATION_UPDATE_DATA);
    flags.UsesLr = !!(bits & VideoAV1PictureBits::USES_LR);
    flags.usesChromaLr = !!(bits & VideoAV1PictureBits::USES_CHROMA_LR);
    flags.apply_grain = !!(bits & VideoAV1PictureBits::APPLY_GRAIN);
}

inline void FillVideoAV1PictureFlagsVK(StdVideoEncodeAV1PictureInfoFlags& flags, VideoAV1PictureBits bits) {
    flags.error_resilient_mode = !!(bits & VideoAV1PictureBits::ERROR_RESILIENT_MODE);
    flags.disable_cdf_update = !!(bits & VideoAV1PictureBits::DISABLE_CDF_UPDATE);
    flags.use_superres = !!(bits & VideoAV1PictureBits::USE_SUPERRES);
    flags.render_and_frame_size_different = !!(bits & VideoAV1PictureBits::RENDER_AND_FRAME_SIZE_DIFFERENT);
    flags.allow_screen_content_tools = !!(bits & VideoAV1PictureBits::ALLOW_SCREEN_CONTENT_TOOLS);
    flags.is_filter_switchable = !!(bits & VideoAV1PictureBits::IS_FILTER_SWITCHABLE);
    flags.force_integer_mv = !!(bits & VideoAV1PictureBits::FORCE_INTEGER_MV);
    flags.frame_size_override_flag = !!(bits & VideoAV1PictureBits::FRAME_SIZE_OVERRIDE);
    flags.buffer_removal_time_present_flag = !!(bits & VideoAV1PictureBits::BUFFER_REMOVAL_TIME_PRESENT);
    flags.allow_intrabc = !!(bits & VideoAV1PictureBits::ALLOW_INTRABC);
    flags.frame_refs_short_signaling = !!(bits & VideoAV1PictureBits::FRAME_REFS_SHORT_SIGNALING);
    flags.allow_high_precision_mv = !!(bits & VideoAV1PictureBits::ALLOW_HIGH_PRECISION_MV);
    flags.is_motion_mode_switchable = !!(bits & VideoAV1PictureBits::IS_MOTION_MODE_SWITCHABLE);
    flags.use_ref_frame_mvs = !!(bits & VideoAV1PictureBits::USE_REF_FRAME_MVS);
    flags.disable_frame_end_update_cdf = !!(bits & VideoAV1PictureBits::DISABLE_FRAME_END_UPDATE_CDF);
    flags.allow_warped_motion = !!(bits & VideoAV1PictureBits::ALLOW_WARPED_MOTION);
    flags.reduced_tx_set = !!(bits & VideoAV1PictureBits::REDUCED_TX_SET);
    flags.skip_mode_present = !!(bits & VideoAV1PictureBits::SKIP_MODE_PRESENT);
    flags.delta_q_present = !!(bits & VideoAV1PictureBits::DELTA_Q_PRESENT);
    flags.delta_lf_present = !!(bits & VideoAV1PictureBits::DELTA_LF_PRESENT);
    flags.delta_lf_multi = !!(bits & VideoAV1PictureBits::DELTA_LF_MULTI);
    flags.segmentation_enabled = !!(bits & VideoAV1PictureBits::SEGMENTATION_ENABLED);
    flags.segmentation_update_map = !!(bits & VideoAV1PictureBits::SEGMENTATION_UPDATE_MAP);
    flags.segmentation_temporal_update = !!(bits & VideoAV1PictureBits::SEGMENTATION_TEMPORAL_UPDATE);
    flags.segmentation_update_data = !!(bits & VideoAV1PictureBits::SEGMENTATION_UPDATE_DATA);
    flags.UsesLr = !!(bits & VideoAV1PictureBits::USES_LR);
    flags.usesChromaLr = !!(bits & VideoAV1PictureBits::USES_CHROMA_LR);
    flags.show_frame = !!(bits & VideoAV1PictureBits::SHOW_FRAME);
    flags.showable_frame = !!(bits & VideoAV1PictureBits::SHOWABLE_FRAME);
}

inline StdVideoAV1FrameType GetVideoAV1FrameTypeVK(VideoEncodeFrameType frameType) {
    switch (frameType) {
    case VideoEncodeFrameType::IDR:
    case VideoEncodeFrameType::I:
        return STD_VIDEO_AV1_FRAME_TYPE_KEY;
    case VideoEncodeFrameType::P:
    case VideoEncodeFrameType::B:
        return STD_VIDEO_AV1_FRAME_TYPE_INTER;
    case VideoEncodeFrameType::MAX_NUM:
        return STD_VIDEO_AV1_FRAME_TYPE_INVALID;
    }

    return STD_VIDEO_AV1_FRAME_TYPE_INVALID;
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

inline const VideoH264ReferenceDesc* FindVideoH264ReferenceDescVK(const VideoH264ReferenceDesc* references, uint32_t referenceNum, uint32_t slot) {
    if (!references)
        return nullptr;

    for (uint32_t i = 0; i < referenceNum; i++) {
        if (references[i].slot == slot)
            return &references[i];
    }

    return nullptr;
}

inline const VideoAV1ReferenceDesc* FindVideoAV1ReferenceDescVK(const VideoAV1ReferenceDesc* references, uint32_t referenceNum, uint32_t slot) {
    if (!references)
        return nullptr;

    for (uint32_t i = 0; i < referenceNum; i++) {
        if (references[i].slot == slot)
            return &references[i];
    }

    return nullptr;
}

inline void FillVideoDecodeAV1ReferenceInfoVK(StdVideoDecodeAV1ReferenceInfo& info, VideoEncodeFrameType frameType, uint8_t orderHint) {
    info = {};
    info.frame_type = (uint8_t)GetVideoAV1FrameTypeVK(frameType);
    info.OrderHint = orderHint;
    for (uint8_t& savedOrderHint : info.SavedOrderHints)
        savedOrderHint = orderHint;
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
