// Copyright (c) 2026 NVIDIA Corporation

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstdint>
#include <cstring>
#include <vector>

// clang-format off
#include "SharedExternal.h" // must come first: its D3D headers include "windows.h", which defines "ERROR" (see "NRIMacro.h")
// clang-format on
#include "NRI.h"

#include "Extensions/NRIVideo.h"

namespace {

struct BitWriter {
    std::vector<uint8_t> bytes;
    uint32_t bitOffset = 0;

    void WriteBit(uint32_t bit) {
        const uint32_t bitInByte = bitOffset % 8;

        if (bitInByte == 0)
            bytes.push_back(0);

        if (bit)
            bytes.back() |= uint8_t(1u << (7u - bitInByte));
        bitOffset++;
    }

    void WriteBits(uint32_t value, uint32_t bitCount) {
        uint32_t i = 0;

        for (; i < bitCount; i++)
            WriteBit((value >> (bitCount - 1u - i)) & 1u);
    }

    void ByteAlign() {
        constexpr uint32_t byteBitNum = 8;

        while (bitOffset % byteBitNum)
            WriteBit(0);
    }
};

std::vector<uint8_t> MakeFrameObu(const std::vector<uint8_t>& payload) {
    std::vector<uint8_t> obu;
    obu.push_back((uint8_t(nri::video::av1::ObuType::Frame) << 3u) | 0x02u);
    size_t value = payload.size();
    do {
        uint8_t byte = uint8_t(value & 0x7Fu);
        value >>= 7u;

        if (value)
            byte |= 0x80u;
        obu.push_back(byte);
    } while (value);
    obu.insert(obu.end(), payload.begin(), payload.end());

    return obu;
}

std::vector<uint8_t> MakeInterFramePayload(uint8_t baseQIndex = 32, bool hasCdef = false) {
    BitWriter writer;
    writer.WriteBit(0);     // show_existing_frame
    writer.WriteBits(1, 2); // INTER_FRAME
    writer.WriteBit(1);     // show_frame
    writer.WriteBit(0);     // error_resilient_mode
    writer.WriteBit(0);     // disable_cdf_update
    writer.WriteBit(0);     // frame_size_override_flag
    writer.WriteBits(9, 8); // order_hint
    writer.WriteBits(3, 3); // primary_ref_frame
    writer.WriteBits(0x24, 8);
    writer.WriteBit(0); // frame_refs_short_signaling

    for (uint32_t i = 0; i < 7; i++)
        writer.WriteBits(i, 3);
    writer.WriteBit(0); // render_and_frame_size_different
    writer.WriteBit(0); // allow_high_precision_mv
    writer.WriteBit(1); // is_filter_switchable
    writer.WriteBit(0); // is_motion_mode_switchable
    writer.WriteBit(0); // disable_frame_end_update_cdf
    writer.WriteBit(1); // uniform_tile_spacing_flag
    writer.WriteBits(baseQIndex, 8);
    writer.WriteBit(0);
    writer.WriteBit(0);
    writer.WriteBit(0);
    writer.WriteBit(0); // using_qmatrix
    writer.WriteBit(0); // segmentation_enabled

    if (baseQIndex) {
        writer.WriteBit(0);     // delta_q_present
        writer.WriteBits(0, 6); // loop_filter_level[0]
        writer.WriteBits(0, 6); // loop_filter_level[1]
        writer.WriteBits(0, 3); // loop_filter_sharpness
        writer.WriteBit(0);     // loop_filter_delta_enabled
        if (hasCdef) {
            writer.WriteBits(1, 2); // cdef_damping_minus_3
            writer.WriteBits(1, 2); // cdef_bits: two strengths
            for (uint32_t i = 0; i < 2; i++) {
                writer.WriteBits(5, 4);     // cdef_y_pri_strength
                writer.WriteBits(1 + i, 2); // cdef_y_sec_strength: 1, 2
                writer.WriteBits(2, 4);     // cdef_uv_pri_strength
                writer.WriteBits(3, 2);     // cdef_uv_sec_strength: 3 (stands for 4)
            }
        }
        writer.WriteBit(1);     // tx_mode select
    }
    writer.WriteBit(0); // reference_select

    for (uint32_t i = 0; i < 7; i++)
        writer.WriteBit(0);
    writer.WriteBit(0); // reduced_tx_set
    writer.ByteAlign();
    writer.bytes.push_back(0x80);

    return writer.bytes;
}

nri::VideoAV1SequenceDesc MakeSequence() {
    nri::VideoAV1SequenceDesc sequence = {};
    sequence.flags = nri::VideoAV1SequenceBits::ENABLE_ORDER_HINT;
    sequence.bitDepth = 8;
    sequence.subsamplingX = 1;
    sequence.subsamplingY = 1;
    sequence.maxFrameWidthMinus1 = 63;
    sequence.maxFrameHeightMinus1 = 63;
    sequence.frameWidthBitsMinus1 = 5;
    sequence.frameHeightBitsMinus1 = 5;
    sequence.orderHintBitsMinus1 = 7;

    return sequence;
}

std::array<nri::VideoAV1ReferenceDesc, 8> MakeReferences() {
    std::array<nri::VideoAV1ReferenceDesc, 8> references = {};

    for (uint32_t i = 0; i < references.size(); i++) {
        references[i].slot = i + 10;
        references[i].refFrameIndex = (uint8_t)i;
        references[i].frameType = i == 0 ? nri::VideoFrameType::IDR : nri::VideoFrameType::P;
        references[i].orderHint = (uint8_t)i;
        references[i].frameId = 100 + i;
    }

    return references;
}

static nri::VideoEncodeFeedback MakeFeedback(size_t encodedBitstreamWrittenBytes) {
    nri::VideoEncodeFeedback feedback = {};
    feedback.encodedBitstreamWrittenBytes = encodedBitstreamWrittenBytes;
    feedback.writtenSubregionNum = 1;

    return feedback;
}

static nri::VideoAV1EncodeDecodeInfoDesc MakeDecodeInfoDesc(const nri::VideoEncodeFeedback& feedback, const nri::VideoAV1SequenceDesc& sequence, const std::vector<uint8_t>& payload, size_t payloadHeaderSize, const std::array<nri::VideoAV1ReferenceDesc, 8>& references) {
    nri::VideoAV1EncodeDecodeInfoDesc desc = {};
    desc.feedback = &feedback;
    desc.sequence = &sequence;
    desc.encodedPayloadHeader = payload.data();
    desc.encodedPayloadHeaderSize = payloadHeaderSize;
    desc.references = references.data();
    desc.referenceNum = (uint32_t)references.size();

    return desc;
}

void ClearInternalPointers(nri::VideoAV1EncodeDecodeInfo& info) {
    info.tileLayout.miColumnStarts = nullptr;
    info.tileLayout.miRowStarts = nullptr;
    info.tileLayout.widthInSuperblocksMinus1 = nullptr;
    info.tileLayout.heightInSuperblocksMinus1 = nullptr;
    info.picture.tileLayout = nullptr;
    info.picture.quantization = nullptr;
    info.picture.loopFilter = nullptr;
    info.picture.cdef = nullptr;
    info.picture.segmentation = nullptr;
    info.picture.loopRestoration = nullptr;
    info.picture.globalMotion = nullptr;
    info.picture.filmGrain = nullptr;
    info.picture.orderHints = nullptr;
    info.picture.tiles = nullptr;
    info.picture.references = nullptr;

    for (nri::VideoAV1ReferenceDesc& reference : info.references)
        reference.savedOrderHints = nullptr;
}

// Configurable INTER frame header (64x64 sequence of "MakeSequence", one tile), written in "uncompressed_header" order
struct InterHeader {
    bool errorResilient = false;
    uint32_t primaryRefFrame = 7; // PRIMARY_REF_NONE
    uint32_t refreshFrameFlags = 0x01;
    bool shortSignaling = false;
    uint32_t lastFrameIdx = 0;
    uint32_t goldFrameIdx = 0;
    bool frameSizeOverride = false;
    int32_t foundRef = -1;      // "frame_size_with_refs": index of the reference with "found_ref", -1 for none
    uint32_t frameWidthMinus1 = 47;
    uint32_t frameHeightMinus1 = 31;
    bool allowHighPrecisionMv = false;
    bool enableRefFrameMvs = false;   // sequence "enable_ref_frame_mvs", "use_ref_frame_mvs = 0" coded if not error resilient
    bool enableWarpedMotion = false;  // sequence "enable_warped_motion", "allow_warped_motion = 0" coded if not error resilient
    uint32_t baseQIndex = 32;
    int32_t segment1AltQ = -20;
    bool segmentation = false;
    bool segmentationUpdateData = true;
    bool loopFilterDeltaEnabled = false;
    bool loopFilterDeltaUpdate = false;
    int32_t loopFilterRefDelta1 = 0; // written to "loop_filter_ref_deltas[1]" if "loopFilterDeltaUpdate"
    bool globalRotZoomLast = false;  // LAST_FRAME: ROTZOOM with "decode_subexp" values 0 (= PrevGmParams) except param 3 (1 = previous - 2)
    bool filmGrain = false;          // requires FILM_GRAIN_PARAMS_PRESENT
    bool updateGrain = true;
    uint32_t grainSeed = 0x1234;
    uint32_t filmGrainRefIdx = 0;
};

std::vector<uint8_t> WriteInterHeader(const InterHeader& header, bool hasFilmGrainParams) {
    BitWriter writer;
    writer.WriteBit(0);     // show_existing_frame
    writer.WriteBits(1, 2); // INTER_FRAME
    writer.WriteBit(1);     // show_frame
    writer.WriteBit(header.errorResilient);
    writer.WriteBit(0); // disable_cdf_update
    writer.WriteBit(header.frameSizeOverride);
    writer.WriteBits(9, 8); // order_hint

    if (!header.errorResilient)
        writer.WriteBits(header.primaryRefFrame, 3);
    writer.WriteBits(header.refreshFrameFlags, 8);

    if (header.errorResilient) {
        for (uint32_t i = 0; i < 8; i++)
            writer.WriteBits(i, 8); // ref_order_hint
    }

    writer.WriteBit(header.shortSignaling);
    if (header.shortSignaling) {
        writer.WriteBits(header.lastFrameIdx, 3);
        writer.WriteBits(header.goldFrameIdx, 3);
    } else {
        for (uint32_t i = 0; i < 7; i++)
            writer.WriteBits(i, 3); // ref_frame_idx
    }

    if (header.frameSizeOverride && !header.errorResilient) {
        for (int32_t i = 0; i < 7; i++) {
            writer.WriteBit(i == header.foundRef);
            if (i == header.foundRef)
                break;
        }
    }

    if (header.frameSizeOverride && (header.errorResilient || header.foundRef < 0)) {
        writer.WriteBits(header.frameWidthMinus1, 6);
        writer.WriteBits(header.frameHeightMinus1, 6);
    }

    if (!header.frameSizeOverride || header.errorResilient || header.foundRef < 0)
        writer.WriteBit(0); // render_and_frame_size_different

    writer.WriteBit(header.allowHighPrecisionMv);
    writer.WriteBit(1); // is_filter_switchable
    writer.WriteBit(0); // is_motion_mode_switchable
    if (header.enableRefFrameMvs && !header.errorResilient)
        writer.WriteBit(0); // use_ref_frame_mvs
    writer.WriteBit(0); // disable_frame_end_update_cdf
    writer.WriteBit(1); // uniform_tile_spacing_flag
    writer.WriteBits(header.baseQIndex, 8); // base_q_idx
    writer.WriteBit(0);      // delta_coded (Y DC)
    writer.WriteBit(0);      // delta_coded (U DC)
    writer.WriteBit(0);      // delta_coded (U AC)
    writer.WriteBit(0);      // using_qmatrix

    writer.WriteBit(header.segmentation);
    if (header.segmentation) {
        if (header.primaryRefFrame != 7 && !header.errorResilient) {
            writer.WriteBit(1); // segmentation_update_map
            writer.WriteBit(0); // segmentation_temporal_update
            writer.WriteBit(header.segmentationUpdateData);
        }

        if (header.segmentationUpdateData || header.primaryRefFrame == 7 || header.errorResilient) {
            for (uint32_t i = 0; i < 8; i++) {
                for (uint32_t j = 0; j < 8; j++) {
                    const bool isEnabled = (i == 1 && (j == 0 || j == 5)) || (i == 2 && j == 1);
                    writer.WriteBit(isEnabled);

                    if (!isEnabled)
                        continue;

                    if (j == 0)
                        writer.WriteBits(0x1FF & uint32_t(header.segment1AltQ), 9); // ALT_Q: su(1 + 8)
                    else if (j == 1)
                        writer.WriteBits(0x7F & uint32_t(-64), 7); // ALT_LF_Y_V: su(1 + 6) = -64 is out of range, clipped to -63
                    else
                        writer.WriteBits(5, 3); // REF_FRAME: f(3) = 5
                }
            }
        }
    }

    if (header.baseQIndex > 0)
        writer.WriteBit(0); // delta_q_present
    writer.WriteBits(8, 6); // loop_filter_level[0]
    writer.WriteBits(8, 6); // loop_filter_level[1]
    writer.WriteBits(4, 6); // loop_filter_level[2]
    writer.WriteBits(4, 6); // loop_filter_level[3]
    writer.WriteBits(0, 3); // loop_filter_sharpness
    writer.WriteBit(header.loopFilterDeltaEnabled);
    if (header.loopFilterDeltaEnabled) {
        writer.WriteBit(header.loopFilterDeltaUpdate);
        if (header.loopFilterDeltaUpdate) {
            for (uint32_t i = 0; i < 8; i++) {
                writer.WriteBit(i == 1);
                if (i == 1)
                    writer.WriteBits(0x7F & uint32_t(header.loopFilterRefDelta1), 7);
            }
            for (uint32_t i = 0; i < 2; i++)
                writer.WriteBit(0);
        }
    }

    writer.WriteBit(1); // tx_mode_select
    writer.WriteBit(0); // reference_select
    if (header.enableWarpedMotion && !header.errorResilient)
        writer.WriteBit(0); // allow_warped_motion
    writer.WriteBit(1); // reduced_tx_set

    for (uint32_t i = 0; i < 7; i++) {
        const bool isGlobal = i == 0 && header.globalRotZoomLast;
        writer.WriteBit(isGlobal);

        if (isGlobal) {
            writer.WriteBit(1); // is_rot_zoom
            // "decode_subexp": subexp_more_bits = 0, subexp_bits f(3); params 2, 3, 0, 1
            writer.WriteBit(0);
            writer.WriteBits(0, 3);
            writer.WriteBit(0);
            writer.WriteBits(1, 3);
            writer.WriteBit(0);
            writer.WriteBits(0, 3);
            writer.WriteBit(0);
            writer.WriteBits(0, 3);
        }
    }

    if (hasFilmGrainParams) {
        writer.WriteBit(header.filmGrain);
        if (header.filmGrain) {
            writer.WriteBits(header.grainSeed, 16);
            writer.WriteBit(header.updateGrain);
            if (!header.updateGrain)
                writer.WriteBits(header.filmGrainRefIdx, 3);
            else {
                writer.WriteBits(2, 4); // num_y_points
                writer.WriteBits(10, 8);
                writer.WriteBits(20, 8);
                writer.WriteBits(200, 8);
                writer.WriteBits(40, 8);
                writer.WriteBit(0);     // chroma_scaling_from_luma
                writer.WriteBits(1, 4); // num_cb_points
                writer.WriteBits(30, 8);
                writer.WriteBits(50, 8);
                writer.WriteBits(1, 4); // num_cr_points (4:2:0 requires Cr points with Cb points)
                writer.WriteBits(60, 8);
                writer.WriteBits(70, 8);
                writer.WriteBits(3, 2); // grain_scaling_minus_8
                writer.WriteBits(1, 2); // ar_coeff_lag: 4 luma, 5 chroma positions
                for (uint32_t i = 0; i < 4; i++)
                    writer.WriteBits(128 + i, 8);
                for (uint32_t i = 0; i < 5; i++)
                    writer.WriteBits(120 + i, 8); // ar_coeffs_cb_plus_128
                for (uint32_t i = 0; i < 5; i++)
                    writer.WriteBits(140 + i, 8); // ar_coeffs_cr_plus_128
                writer.WriteBits(2, 2); // ar_coeff_shift_minus_6
                writer.WriteBits(1, 2); // grain_scale_shift
                writer.WriteBits(100, 8); // cb_mult
                writer.WriteBits(110, 8); // cb_luma_mult
                writer.WriteBits(300, 9); // cb_offset
                writer.WriteBits(90, 8);  // cr_mult
                writer.WriteBits(95, 8);  // cr_luma_mult
                writer.WriteBits(400, 9); // cr_offset
                writer.WriteBit(1);       // overlap_flag
                writer.WriteBit(0);       // clip_to_restricted_range
            }
        }
    }

    writer.ByteAlign();
    writer.bytes.push_back(0x80);

    return writer.bytes;
}

std::array<nri::VideoAV1SavedFrameState, 8> MakeSavedFrameStates() {
    std::array<nri::VideoAV1SavedFrameState, 8> states = {};

    for (uint32_t i = 0; i < states.size(); i++) {
        nri::VideoAV1SavedFrameState& state = states[i];
        state.valid = 1;
        state.orderHint = (uint8_t)(i + 1);
        state.upscaledWidthMinus1 = (uint16_t)(31 + i);
        state.frameHeightMinus1 = (uint16_t)(15 + i);
        state.renderWidthMinus1 = (uint16_t)(30 + i);
        state.renderHeightMinus1 = (uint16_t)(14 + i);
        state.loopFilter.refDeltas[0] = (int8_t)(10 + i);
        state.loopFilter.refDeltas[1] = (int8_t)(-3 - (int32_t)i);
        state.loopFilter.modeDeltas[1] = (int8_t)(2 + i);
        state.segmentation.featureEnabled[3] = 0x01;
        state.segmentation.featureData[3][0] = (int16_t)(40 + i);
        state.applyGrain = 1;
        state.filmGrain.numYPoints = 1;
        state.filmGrain.pointYValue[0] = (uint8_t)(60 + i);
        state.filmGrain.grainSeed = 7;

        for (uint32_t reference = 0; reference < 8; reference++) {
            state.globalMotion.params[reference][2] = 1 << 16;
            state.globalMotion.params[reference][5] = 1 << 16;
        }
        state.globalMotion.params[1][3] = 512 + (int32_t)i * 2; // LAST_FRAME beta, a multiple of 2 ("precDiff" = 1)
    }

    return states;
}

nri::Result ParseInterHeader(const InterHeader& header, const nri::VideoAV1EncodeDecodeInfoDesc* base, const nri::VideoAV1SavedFrameState* states, nri::VideoAV1EncodeDecodeInfo& info, bool hasFilmGrainParams = false) {
    static thread_local std::vector<uint8_t> payload;
    payload = MakeFrameObu(WriteInterHeader(header, hasFilmGrainParams));

    static thread_local nri::VideoEncodeFeedback feedback;
    feedback = MakeFeedback(payload.size());

    static thread_local nri::VideoAV1SequenceDesc sequence;
    sequence = MakeSequence();
    if (header.enableRefFrameMvs)
        sequence.flags |= nri::VideoAV1SequenceBits::ENABLE_REF_FRAME_MVS;
    if (header.enableWarpedMotion)
        sequence.flags |= nri::VideoAV1SequenceBits::ENABLE_WARPED_MOTION;
    if (hasFilmGrainParams)
        sequence.flags |= nri::VideoAV1SequenceBits::FILM_GRAIN_PARAMS_PRESENT;

    nri::VideoAV1EncodeDecodeInfoDesc desc = *base;
    desc.feedback = &feedback;
    desc.sequence = &sequence;
    desc.encodedPayloadHeader = payload.data();
    desc.encodedPayloadHeaderSize = payload.size();
    desc.savedFrameStates = states;
    info = {};

    return nri::video::av1::GetEncodeDecodeInfoFromHeader(desc, info);
}

} // namespace

TEST_CASE("VID-SER-002 AV1 OBU size queries and bounds are exact", "[video][serializer][short]") {
    nri::VideoAV1ObuHeadersDesc desc = {};
    desc.sequence = MakeSequence();
    REQUIRE(nri::video::WriteAV1ObuHeaders(desc) == nri::Result::SUCCESS);
    REQUIRE(desc.writtenSize != 0);

    std::vector<uint8_t> bytes(desc.writtenSize + 2, 0xA5);
    desc.dst = bytes.data() + 1;
    desc.dstSize = desc.writtenSize - 1;
    REQUIRE(nri::video::WriteAV1ObuHeaders(desc) == nri::Result::INVALID_ARGUMENT);
    REQUIRE(bytes.front() == 0xA5);
    REQUIRE(bytes.back() == 0xA5);

    desc.dstSize = desc.writtenSize;
    REQUIRE(nri::video::WriteAV1ObuHeaders(desc) == nri::Result::SUCCESS);
    REQUIRE(bytes.front() == 0xA5);
    REQUIRE(bytes.back() == 0xA5);
}

TEST_CASE("VID-PAR-001 AV1 inter-frame metadata parses all references", "[video][parser][short]") {
    const std::vector<uint8_t> payload = MakeFrameObu(MakeInterFramePayload());
    const nri::VideoEncodeFeedback feedback = MakeFeedback(payload.size());
    const nri::VideoAV1SequenceDesc sequence = MakeSequence();
    const std::array<nri::VideoAV1ReferenceDesc, 8> references = MakeReferences();
    const nri::VideoAV1EncodeDecodeInfoDesc desc = MakeDecodeInfoDesc(feedback, sequence, payload, payload.size(), references);

    nri::VideoAV1EncodeDecodeInfo info = {};
    REQUIRE(nri::video::av1::GetEncodeDecodeInfoFromHeader(desc, info) == nri::Result::SUCCESS);
    REQUIRE(info.picture.frameType == nri::VideoFrameType::P);
    REQUIRE(info.picture.refreshFrameFlags == 0x24);
    REQUIRE(info.picture.primaryReferenceName == nri::VideoAV1ReferenceName::GOLDEN);
    REQUIRE(info.picture.referenceNum == 8); // 7 names, then the unused reference frame map entry 7
    REQUIRE(info.picture.references[0].slot == 10);
    REQUIRE(info.picture.references[7].name == nri::VideoAV1ReferenceName::NONE);
    REQUIRE(info.picture.references[7].refFrameIndex == 7);
    REQUIRE(info.picture.references[7].slot == 17);
    REQUIRE(info.bitstreamOffset <= payload.size());
    REQUIRE(info.bitstreamSize == payload.size() - info.bitstreamOffset);
    REQUIRE(info.picture.tileNum == 1);
    REQUIRE(info.picture.tiles);

    for (uint32_t i = 0; i < info.picture.tileNum; i++)
        REQUIRE(uint64_t(info.picture.tiles[i].offset) + info.picture.tiles[i].size <= info.bitstreamSize);
}

TEST_CASE("VID-REG-030 AV1 CDEF secondary strengths are passed as coded", "[video][parser][regression][short]") {
    const std::vector<uint8_t> payload = MakeFrameObu(MakeInterFramePayload(32, true));
    const nri::VideoEncodeFeedback feedback = MakeFeedback(payload.size());
    nri::VideoAV1SequenceDesc sequence = MakeSequence();
    sequence.flags |= nri::VideoAV1SequenceBits::ENABLE_CDEF;
    const std::array<nri::VideoAV1ReferenceDesc, 8> references = MakeReferences();
    const nri::VideoAV1EncodeDecodeInfoDesc desc = MakeDecodeInfoDesc(feedback, sequence, payload, payload.size(), references);

    nri::VideoAV1EncodeDecodeInfo info = {};
    REQUIRE(nri::video::av1::GetEncodeDecodeInfoFromHeader(desc, info) == nri::Result::SUCCESS);
    REQUIRE(info.picture.cdefDampingMinus3 == 1);
    REQUIRE(info.picture.cdefBits == 1);
    REQUIRE(info.cdef.yPrimaryStrength[1] == 5);
    REQUIRE(info.cdef.ySecondaryStrength[0] == 1);
    REQUIRE(info.cdef.ySecondaryStrength[1] == 2);
    REQUIRE(info.cdef.uvPrimaryStrength[0] == 2);
    REQUIRE(info.cdef.uvSecondaryStrength[0] == 3);
    REQUIRE(info.cdef.uvSecondaryStrength[1] == 3);
}

TEST_CASE("VID-REG-003 AV1 parse result does not depend on caller bytes", "[video][parser][regression][short]") {
    const std::vector<uint8_t> payload = MakeFrameObu(MakeInterFramePayload());
    const nri::VideoEncodeFeedback feedback = MakeFeedback(payload.size());
    const nri::VideoAV1SequenceDesc sequence = MakeSequence();
    const std::array<nri::VideoAV1ReferenceDesc, 8> references = MakeReferences();
    const nri::VideoAV1EncodeDecodeInfoDesc desc = MakeDecodeInfoDesc(feedback, sequence, payload, payload.size(), references);

    nri::VideoAV1EncodeDecodeInfo zeroed = {};
    nri::VideoAV1EncodeDecodeInfo poisoned;
    std::memset(&poisoned, 0xCD, sizeof(poisoned));
    REQUIRE(nri::video::av1::GetEncodeDecodeInfoFromHeader(desc, zeroed) == nri::Result::SUCCESS);
    REQUIRE(nri::video::av1::GetEncodeDecodeInfoFromHeader(desc, poisoned) == nri::Result::SUCCESS);

    REQUIRE(zeroed.picture.tileLayout == &zeroed.tileLayout);
    REQUIRE(poisoned.picture.tileLayout == &poisoned.tileLayout);
    REQUIRE(zeroed.picture.tiles == zeroed.tiles);
    REQUIRE(poisoned.picture.tiles == poisoned.tiles);

    ClearInternalPointers(zeroed);
    ClearInternalPointers(poisoned);
    REQUIRE(std::memcmp(&zeroed, &poisoned, sizeof(zeroed)) == 0);
}

TEST_CASE("VID-REG-020 invalid AV1 dependent fields are rejected", "[video][serializer][regression][short]") {
    nri::VideoAV1ObuHeadersDesc desc = {};
    desc.sequence = MakeSequence();

    desc.sequence.orderHintBitsMinus1 = 8;
    REQUIRE(nri::video::WriteAV1ObuHeaders(desc) == nri::Result::INVALID_ARGUMENT);

    desc.sequence = MakeSequence();
    desc.sequence.flags |= nri::VideoAV1SequenceBits::TIMING_INFO_PRESENT;
    desc.sequence.numTicksPerPictureMinus1 = UINT32_MAX;
    REQUIRE(nri::video::WriteAV1ObuHeaders(desc) == nri::Result::INVALID_ARGUMENT);

    desc.sequence = MakeSequence();
    desc.sequence.seqProfile = 0;
    desc.sequence.bitDepth = 12;
    REQUIRE(nri::video::WriteAV1ObuHeaders(desc) == nri::Result::INVALID_ARGUMENT);
}

TEST_CASE("VID-PAR-002 AV1 OBU scanner rejects cursor and extension hazards", "[video][parser][short]") {
    nri::video::av1::ObuSpan span = {};
    const std::array<uint8_t, 4> obu = {
        uint8_t((uint8_t(nri::video::av1::ObuType::Frame) << 3u) | 0x06u),
        0x01,
        0x01,
        0x00,
    };

    size_t cursor = obu.size();
    REQUIRE_FALSE(nri::video::av1::ReadObuHeader(obu.data(), obu.size(), cursor, span));

    cursor = obu.size() + 1;
    REQUIRE_FALSE(nri::video::av1::ReadObuHeader(obu.data(), obu.size(), cursor, span));

    cursor = 0;
    REQUIRE_FALSE(nri::video::av1::ReadObuHeader(obu.data(), obu.size(), cursor, span));

    std::array<uint8_t, 4> valid = obu;
    valid[1] = 0;
    cursor = 0;
    REQUIRE(nri::video::av1::ReadObuHeader(valid.data(), valid.size(), cursor, span));
    REQUIRE(span.payloadOffset == 3);
    REQUIRE(span.payloadSize == 1);
}

TEST_CASE("VID-PAR-003 AV1 parser is failure-atomic for every short prefix", "[video][parser][short]") {
    const std::vector<uint8_t> payload = MakeFrameObu(MakeInterFramePayload());
    const nri::VideoEncodeFeedback feedback = MakeFeedback(payload.size());
    const nri::VideoAV1SequenceDesc sequence = MakeSequence();
    const std::array<nri::VideoAV1ReferenceDesc, 8> references = MakeReferences();

    for (size_t prefixSize = 0; prefixSize < payload.size(); prefixSize++) {
        const nri::VideoAV1EncodeDecodeInfoDesc desc = MakeDecodeInfoDesc(feedback, sequence, payload, prefixSize, references);

        nri::VideoAV1EncodeDecodeInfo info;
        std::memset(&info, 0xA5, sizeof(info));
        const nri::VideoAV1EncodeDecodeInfo original = info;
        const nri::Result result = nri::video::av1::GetEncodeDecodeInfoFromHeader(desc, info);

        if (result != nri::Result::SUCCESS)
            REQUIRE(std::memcmp(&info, &original, sizeof(info)) == 0);
        else {
            REQUIRE(info.bitstreamOffset <= feedback.encodedBitstreamWrittenBytes);
            REQUIRE(info.bitstreamSize <= feedback.encodedBitstreamWrittenBytes - info.bitstreamOffset);
        }
    }
}

TEST_CASE("VID-SER-006 AV1 sequence dependency matrix rejects invalid combinations", "[video][serializer][short]") {
    nri::VideoAV1ObuHeadersDesc desc = {};
    desc.sequence = MakeSequence();

    SECTION("reduced still picture dependencies") {
        desc.sequence.flags = nri::VideoAV1SequenceBits::REDUCED_STILL_PICTURE_HEADER;
        REQUIRE(nri::video::WriteAV1ObuHeaders(desc) == nri::Result::INVALID_ARGUMENT);

        desc.sequence.flags |= nri::VideoAV1SequenceBits::STILL_PICTURE | nri::VideoAV1SequenceBits::ENABLE_ORDER_HINT;
        REQUIRE(nri::video::WriteAV1ObuHeaders(desc) == nri::Result::INVALID_ARGUMENT);
    }

    SECTION("profile one color constraints") {
        desc.sequence.seqProfile = 1;
        desc.sequence.subsamplingX = 1;
        REQUIRE(nri::video::WriteAV1ObuHeaders(desc) == nri::Result::INVALID_ARGUMENT);
    }

    SECTION("frame dimensions fit declared widths") {
        desc.sequence.frameWidthBitsMinus1 = 0;
        desc.sequence.maxFrameWidthMinus1 = 2;
        REQUIRE(nri::video::WriteAV1ObuHeaders(desc) == nri::Result::INVALID_ARGUMENT);
    }

    SECTION("screen-content selectors") {
        desc.sequence.seqForceScreenContentTools = 3;
        REQUIRE(nri::video::WriteAV1ObuHeaders(desc) == nri::Result::INVALID_ARGUMENT);
    }

    SECTION("monochrome inferred fields") {
        desc.sequence.flags |= nri::VideoAV1SequenceBits::MONO_CHROME | nri::VideoAV1SequenceBits::SEPARATE_UV_DELTA_Q;
        REQUIRE(nri::video::WriteAV1ObuHeaders(desc) == nri::Result::INVALID_ARGUMENT);
    }

    SECTION("identity-matrix inferred fields") {
        desc.sequence.seqProfile = 1;
        desc.sequence.subsamplingX = 0;
        desc.sequence.subsamplingY = 0;
        desc.sequence.flags |= nri::VideoAV1SequenceBits::COLOR_DESCRIPTION_PRESENT;
        desc.sequence.colorPrimaries = 1;           // BT.709
        desc.sequence.transferCharacteristics = 13; // sRGB
        desc.sequence.matrixCoefficients = 0;       // identity
        REQUIRE(nri::video::WriteAV1ObuHeaders(desc) == nri::Result::INVALID_ARGUMENT);
    }
}

TEST_CASE("VID-SER-008 AV1 representative OBU headers match golden bytes", "[video][serializer][short]") {
    nri::VideoAV1ObuHeadersDesc desc = {};
    desc.sequence = MakeSequence();
    REQUIRE(nri::video::WriteAV1ObuHeaders(desc) == nri::Result::SUCCESS);

    const std::array<uint8_t, 14> golden = {
        0x12,
        0x00,
        0x0A,
        0x0A,
        0x00,
        0x00,
        0x00,
        0x02,
        0xAF,
        0xFF,
        0x80,
        0x43,
        0x80,
        0x08,
    };
    REQUIRE(desc.writtenSize == golden.size());

    std::array<uint8_t, 14> bytes = {};
    desc.dst = bytes.data();
    desc.dstSize = bytes.size();
    REQUIRE(nri::video::WriteAV1ObuHeaders(desc) == nri::Result::SUCCESS);
    REQUIRE(bytes == golden);
}

TEST_CASE("VID-REG-027 AV1 inferred color syntax matches the specification", "[video][serializer][regression][short]") {
    SECTION("monochrome") {
        nri::VideoAV1ObuHeadersDesc desc = {};
        desc.sequence = MakeSequence();
        desc.sequence.flags |= nri::VideoAV1SequenceBits::MONO_CHROME;

        const std::array<uint8_t, 14> golden = {
            0x12,
            0x00,
            0x0A,
            0x0A,
            0x00,
            0x00,
            0x00,
            0x02,
            0xAF,
            0xFF,
            0x80,
            0x43,
            0x84,
            0x40,
        };
        std::array<uint8_t, 14> bytes = {};
        desc.dst = bytes.data();
        desc.dstSize = bytes.size();
        REQUIRE(nri::video::WriteAV1ObuHeaders(desc) == nri::Result::SUCCESS);
        REQUIRE(desc.writtenSize == golden.size());
        REQUIRE(bytes == golden);
    }

    SECTION("BT.709 sRGB identity matrix") {
        nri::VideoAV1ObuHeadersDesc desc = {};
        desc.sequence = MakeSequence();
        desc.sequence.seqProfile = 1;
        desc.sequence.subsamplingX = 0;
        desc.sequence.subsamplingY = 0;
        desc.sequence.flags |= nri::VideoAV1SequenceBits::COLOR_DESCRIPTION_PRESENT | nri::VideoAV1SequenceBits::COLOR_RANGE;
        desc.sequence.colorPrimaries = 1;           // BT.709
        desc.sequence.transferCharacteristics = 13; // sRGB
        desc.sequence.matrixCoefficients = 0;       // identity

        const std::array<uint8_t, 17> golden = {
            0x12,
            0x00,
            0x0A,
            0x0D,
            0x20,
            0x00,
            0x00,
            0x02,
            0xAF,
            0xFF,
            0x80,
            0x43,
            0x84,
            0x04,
            0x34,
            0x00,
            0x80,
        };
        std::array<uint8_t, 17> bytes = {};
        desc.dst = bytes.data();
        desc.dstSize = bytes.size();
        REQUIRE(nri::video::WriteAV1ObuHeaders(desc) == nri::Result::SUCCESS);
        REQUIRE(desc.writtenSize == golden.size());
        REQUIRE(bytes == golden);
    }
}

TEST_CASE("VID-PAR-004 AV1 error resilient INTER frames parse with implied PRIMARY_REF_NONE", "[video][parser][short]") {
    const std::array<nri::VideoAV1ReferenceDesc, 8> references = MakeReferences();
    nri::VideoAV1EncodeDecodeInfoDesc base = {};
    base.references = references.data();
    base.referenceNum = 8;

    InterHeader header;
    header.errorResilient = true;
    header.loopFilterDeltaEnabled = true; // defaults ("setup_past_independence"), no state needed

    nri::VideoAV1EncodeDecodeInfo info = {};
    REQUIRE(ParseInterHeader(header, &base, nullptr, info) == nri::Result::SUCCESS);
    REQUIRE((info.picture.flags & nri::VideoAV1PictureBits::ERROR_RESILIENT_MODE));
    REQUIRE(info.picture.primaryReferenceName == nri::VideoAV1ReferenceName::NONE);
    REQUIRE(info.loopFilter.refDeltas[0] == 1);
    REQUIRE(info.loopFilter.refDeltas[7] == -1);
    REQUIRE(info.savedFrameState.valid == 1);
}

TEST_CASE("VID-PAR-005 AV1 loop filter deltas are inherited from primary_ref_frame", "[video][parser][short]") {
    const std::array<nri::VideoAV1ReferenceDesc, 8> references = MakeReferences();
    const std::array<nri::VideoAV1SavedFrameState, 8> states = MakeSavedFrameStates();
    nri::VideoAV1EncodeDecodeInfoDesc base = {};
    base.references = references.data();
    base.referenceNum = 8;

    InterHeader header;
    header.primaryRefFrame = 3; // GOLDEN_FRAME: ref_frame_idx[3] = 3
    header.loopFilterDeltaEnabled = true;

    nri::VideoAV1EncodeDecodeInfo info = {};

    SECTION("inherited") {
        REQUIRE(ParseInterHeader(header, &base, states.data(), info) == nri::Result::SUCCESS);
        REQUIRE(info.loopFilter.refDeltas[0] == states[3].loopFilter.refDeltas[0]);
        REQUIRE(info.loopFilter.refDeltas[1] == states[3].loopFilter.refDeltas[1]);
        REQUIRE(info.loopFilter.modeDeltas[1] == states[3].loopFilter.modeDeltas[1]);
        REQUIRE(info.savedFrameState.valid == 1);
        REQUIRE(info.savedFrameState.loopFilter.refDeltas[0] == states[3].loopFilter.refDeltas[0]);
    }
    SECTION("inherited and updated") {
        header.loopFilterDeltaUpdate = true;
        header.loopFilterRefDelta1 = 9;
        REQUIRE(ParseInterHeader(header, &base, states.data(), info) == nri::Result::SUCCESS);
        REQUIRE(info.loopFilter.refDeltas[0] == states[3].loopFilter.refDeltas[0]);
        REQUIRE(info.loopFilter.refDeltas[1] == 9);
    }
    SECTION("missing state is rejected when the deltas are used") {
        REQUIRE(ParseInterHeader(header, &base, nullptr, info) == nri::Result::FAILURE);

        std::array<nri::VideoAV1SavedFrameState, 8> invalidStates = states;
        invalidStates[3].valid = 0;
        REQUIRE(ParseInterHeader(header, &base, invalidStates.data(), info) == nri::Result::FAILURE);
    }
    SECTION("missing state without using the deltas invalidates the saved state") {
        header.loopFilterDeltaEnabled = false;
        REQUIRE(ParseInterHeader(header, &base, nullptr, info) == nri::Result::SUCCESS);
        REQUIRE(info.savedFrameState.valid == 0);
    }
}

TEST_CASE("VID-PAR-006 AV1 segmentation parameters are parsed or inherited", "[video][parser][short]") {
    const std::array<nri::VideoAV1ReferenceDesc, 8> references = MakeReferences();
    const std::array<nri::VideoAV1SavedFrameState, 8> states = MakeSavedFrameStates();
    nri::VideoAV1EncodeDecodeInfoDesc base = {};
    base.references = references.data();
    base.referenceNum = 8;

    InterHeader header;
    header.segmentation = true;
    nri::VideoAV1EncodeDecodeInfo info = {};

    SECTION("coded") {
        REQUIRE(ParseInterHeader(header, &base, nullptr, info) == nri::Result::SUCCESS);
        REQUIRE((info.picture.flags & nri::VideoAV1PictureBits::SEGMENTATION_ENABLED));
        REQUIRE((info.picture.flags & nri::VideoAV1PictureBits::SEGMENTATION_UPDATE_MAP));
        REQUIRE((info.picture.flags & nri::VideoAV1PictureBits::SEGMENTATION_UPDATE_DATA));
        REQUIRE(info.segmentation.featureEnabled[1] == ((1u << 0) | (1u << 5)));
        REQUIRE(info.segmentation.featureData[1][0] == -20);
        REQUIRE(info.segmentation.featureData[1][5] == 5);
        REQUIRE(info.segmentation.featureEnabled[2] == (1u << 1));
        REQUIRE(info.segmentation.featureData[2][1] == -63); // clipped
        REQUIRE(info.picture.segmentation == &info.segmentation);
        REQUIRE(info.savedFrameState.segmentation.featureData[1][0] == -20);
    }
    SECTION("inherited") {
        header.primaryRefFrame = 1; // LAST2_FRAME: ref_frame_idx[1] = 1
        header.segmentationUpdateData = false;
        REQUIRE(ParseInterHeader(header, &base, states.data(), info) == nri::Result::SUCCESS);
        REQUIRE(!(info.picture.flags & nri::VideoAV1PictureBits::SEGMENTATION_UPDATE_DATA));
        REQUIRE(info.segmentation.featureEnabled[3] == states[1].segmentation.featureEnabled[3]);
        REQUIRE(info.segmentation.featureData[3][0] == states[1].segmentation.featureData[3][0]);
        REQUIRE(ParseInterHeader(header, &base, nullptr, info) == nri::Result::FAILURE);
    }
}

TEST_CASE("VID-PAR-007 AV1 global motion is coded relative to the primary reference", "[video][parser][short]") {
    const std::array<nri::VideoAV1ReferenceDesc, 8> references = MakeReferences();
    const std::array<nri::VideoAV1SavedFrameState, 8> states = MakeSavedFrameStates();
    nri::VideoAV1EncodeDecodeInfoDesc base = {};
    base.references = references.data();
    base.referenceNum = 8;

    InterHeader header;
    header.globalRotZoomLast = true;
    nri::VideoAV1EncodeDecodeInfo info = {};

    SECTION("identity previous parameters") {
        REQUIRE(ParseInterHeader(header, &base, nullptr, info) == nri::Result::SUCCESS);
        REQUIRE(info.globalMotion.type[1] == 2); // ROTZOOM
        REQUIRE(info.globalMotion.params[1][2] == (1 << 16));
        REQUIRE(info.globalMotion.params[1][3] == -2); // "inverse_recenter(r, 1)" = r - 1, scaled by "precDiff" = 1
        REQUIRE(info.globalMotion.params[1][4] == 2);
        REQUIRE(info.globalMotion.params[1][5] == (1 << 16));
        REQUIRE(info.globalMotion.params[1][0] == 0);
        REQUIRE(info.globalMotion.params[2][2] == (1 << 16)); // other references stay identity
        REQUIRE(info.globalMotion.invalid[1] == 0);
    }
    SECTION("previous parameters of primary_ref_frame") {
        header.primaryRefFrame = 0; // LAST_FRAME: ref_frame_idx[0] = 0
        REQUIRE(ParseInterHeader(header, &base, states.data(), info) == nri::Result::SUCCESS);
        // The reference is in the upper half of the "decode_signed_subexp_with_ref" range: the same coded value is mirrored, + 1 step
        REQUIRE(info.globalMotion.params[1][3] == states[0].globalMotion.params[1][3] + 2);
        REQUIRE(ParseInterHeader(header, &base, nullptr, info) == nri::Result::FAILURE);
    }
}

TEST_CASE("VID-PAR-008 AV1 film grain parameters are parsed or loaded", "[video][parser][short]") {
    const std::array<nri::VideoAV1ReferenceDesc, 8> references = MakeReferences();
    const std::array<nri::VideoAV1SavedFrameState, 8> states = MakeSavedFrameStates();
    nri::VideoAV1EncodeDecodeInfoDesc base = {};
    base.references = references.data();
    base.referenceNum = 8;

    InterHeader header;
    header.filmGrain = true;
    nri::VideoAV1EncodeDecodeInfo info = {};

    SECTION("update_grain = 1") {
        REQUIRE(ParseInterHeader(header, &base, nullptr, info, true) == nri::Result::SUCCESS);
        REQUIRE((info.picture.flags & nri::VideoAV1PictureBits::APPLY_GRAIN));
        REQUIRE(info.picture.filmGrain == &info.filmGrain);
        REQUIRE(info.filmGrain.grainSeed == 0x1234);
        REQUIRE(info.filmGrain.numYPoints == 2);
        REQUIRE(info.filmGrain.pointYValue[1] == 200);
        REQUIRE(info.filmGrain.pointYScaling[1] == 40);
        REQUIRE(info.filmGrain.numCbPoints == 1);
        REQUIRE(info.filmGrain.numCrPoints == 1);
        REQUIRE(info.filmGrain.pointCrValue[0] == 60);
        REQUIRE(info.filmGrain.pointCrScaling[0] == 70);
        REQUIRE(info.filmGrain.arCoeffsCrPlus128[4] == 144);
        REQUIRE(info.filmGrain.crMult == 90);
        REQUIRE(info.filmGrain.crLumaMult == 95);
        REQUIRE(info.filmGrain.crOffset == 400);
        REQUIRE(info.filmGrain.grainScalingMinus8 == 3);
        REQUIRE(info.filmGrain.arCoeffLag == 1);
        REQUIRE(info.filmGrain.arCoeffsYPlus128[3] == 131);
        REQUIRE(info.filmGrain.arCoeffsCbPlus128[4] == 124);
        REQUIRE(info.filmGrain.cbOffset == 300);
        REQUIRE(info.filmGrain.overlapFlag == 1);
        REQUIRE(info.savedFrameState.filmGrain.pointYValue[1] == 200);
        REQUIRE(info.savedFrameState.applyGrain == 1);
    }
    SECTION("update_grain = 0 loads the reference parameters with the new seed") {
        header.updateGrain = false;
        header.filmGrainRefIdx = 4;
        REQUIRE(ParseInterHeader(header, &base, states.data(), info, true) == nri::Result::SUCCESS);
        REQUIRE(info.filmGrain.pointYValue[0] == states[4].filmGrain.pointYValue[0]);
        REQUIRE(info.filmGrain.grainSeed == 0x1234);
        REQUIRE(info.filmGrain.updateGrain == 1);
        REQUIRE((info.picture.flags & nri::VideoAV1PictureBits::APPLY_GRAIN));
        REQUIRE(ParseInterHeader(header, &base, nullptr, info, true) == nri::Result::FAILURE);

        header.filmGrainRefIdx = 7; // not one of "ref_frame_idx[]" (0..6)
        REQUIRE(ParseInterHeader(header, &base, states.data(), info, true) == nri::Result::FAILURE);
    }
    SECTION("update_grain = 0 from a reference without grain restores apply_grain = 0") {
        std::array<nri::VideoAV1SavedFrameState, 8> noGrainStates = states;
        noGrainStates[4].applyGrain = 0;
        noGrainStates[4].filmGrain = {};
        header.updateGrain = false;
        header.filmGrainRefIdx = 4;
        REQUIRE(ParseInterHeader(header, &base, noGrainStates.data(), info, true) == nri::Result::SUCCESS);
        REQUIRE(!(info.picture.flags & nri::VideoAV1PictureBits::APPLY_GRAIN));
        REQUIRE(info.filmGrain.grainSeed == 0);
        REQUIRE(info.savedFrameState.applyGrain == 0);
    }
    SECTION("no grain") {
        header.filmGrain = false;
        REQUIRE(ParseInterHeader(header, &base, nullptr, info, true) == nri::Result::SUCCESS);
        REQUIRE(!(info.picture.flags & nri::VideoAV1PictureBits::APPLY_GRAIN));
        REQUIRE(info.filmGrain.numYPoints == 0);
    }
}

TEST_CASE("VID-PAR-009 AV1 frame_size_with_refs takes the size of found_ref", "[video][parser][short]") {
    const std::array<nri::VideoAV1ReferenceDesc, 8> references = MakeReferences();
    const std::array<nri::VideoAV1SavedFrameState, 8> states = MakeSavedFrameStates();
    nri::VideoAV1EncodeDecodeInfoDesc base = {};
    base.references = references.data();
    base.referenceNum = 8;

    InterHeader header;
    header.frameSizeOverride = true;
    nri::VideoAV1EncodeDecodeInfo info = {};

    SECTION("found_ref") {
        header.foundRef = 2; // ref_frame_idx[2] = 2
        REQUIRE(ParseInterHeader(header, &base, states.data(), info) == nri::Result::SUCCESS);
        REQUIRE(info.savedFrameState.upscaledWidthMinus1 == states[2].upscaledWidthMinus1);
        REQUIRE(info.savedFrameState.frameHeightMinus1 == states[2].frameHeightMinus1);
        REQUIRE(info.picture.renderWidthMinus1 == states[2].renderWidthMinus1);
        REQUIRE(info.picture.renderHeightMinus1 == states[2].renderHeightMinus1);
        REQUIRE(ParseInterHeader(header, &base, nullptr, info) == nri::Result::FAILURE);
    }
    SECTION("no found_ref") {
        REQUIRE(ParseInterHeader(header, &base, nullptr, info) == nri::Result::SUCCESS);
        REQUIRE(info.savedFrameState.upscaledWidthMinus1 == 47);
        REQUIRE(info.savedFrameState.frameHeightMinus1 == 31);
        REQUIRE(info.picture.renderWidthMinus1 == 47);
    }
}

TEST_CASE("VID-PAR-010 AV1 frame_refs_short_signaling derives the references", "[video][parser][short]") {
    const std::array<nri::VideoAV1ReferenceDesc, 8> references = MakeReferences();
    const std::array<nri::VideoAV1SavedFrameState, 8> states = MakeSavedFrameStates(); // order hints 1..8, current 9: all forward
    nri::VideoAV1EncodeDecodeInfoDesc base = {};
    base.references = references.data();
    base.referenceNum = 8;

    InterHeader header;
    header.shortSignaling = true;
    header.lastFrameIdx = 7;
    header.goldFrameIdx = 0;
    nri::VideoAV1EncodeDecodeInfo info = {};

    REQUIRE(ParseInterHeader(header, &base, states.data(), info) == nri::Result::SUCCESS);
    REQUIRE((info.picture.flags & nri::VideoAV1PictureBits::FRAME_REFS_SHORT_SIGNALING));

    // LAST = 7, GOLDEN = 0; LAST2, LAST3, BWDREF, ALTREF2, ALTREF take the latest remaining forward slots 6, 5, 4, 3, 2
    constexpr uint32_t expected[7] = {7, 6, 5, 0, 4, 3, 2};
    for (uint32_t i = 0; i < 7; i++) {
        CAPTURE(i);
        REQUIRE(info.picture.references[i].refFrameIndex == expected[i]);
        REQUIRE(info.picture.references[i].slot == references[expected[i]].slot);
    }

    REQUIRE(ParseInterHeader(header, &base, nullptr, info) == nri::Result::FAILURE);
}

TEST_CASE("VID-PAR-011 AV1 state-dependent headers are failure-atomic for every short prefix", "[video][parser][short]") {
    const std::array<nri::VideoAV1ReferenceDesc, 8> references = MakeReferences();
    const std::array<nri::VideoAV1SavedFrameState, 8> states = MakeSavedFrameStates();

    InterHeader header;
    header.primaryRefFrame = 0;
    header.segmentation = true;
    header.loopFilterDeltaEnabled = true;
    header.loopFilterDeltaUpdate = true;
    header.globalRotZoomLast = true;
    header.filmGrain = true;

    const std::vector<uint8_t> payload = MakeFrameObu(WriteInterHeader(header, true));
    const nri::VideoEncodeFeedback feedback = MakeFeedback(payload.size());
    nri::VideoAV1SequenceDesc sequence = MakeSequence();
    sequence.flags |= nri::VideoAV1SequenceBits::FILM_GRAIN_PARAMS_PRESENT;
    nri::VideoAV1EncodeDecodeInfoDesc desc = MakeDecodeInfoDesc(feedback, sequence, payload, payload.size(), references);
    desc.savedFrameStates = states.data();

    nri::VideoAV1EncodeDecodeInfo info = {};
    REQUIRE(nri::video::av1::GetEncodeDecodeInfoFromHeader(desc, info) == nri::Result::SUCCESS);

    for (size_t size = 1; size < payload.size() - 1; size++) {
        CAPTURE(size);
        desc.encodedPayloadHeaderSize = size;
        nri::VideoAV1EncodeDecodeInfo prefixInfo;
        std::memset(&prefixInfo, 0xA5, sizeof(prefixInfo));
        const nri::VideoAV1EncodeDecodeInfo original = prefixInfo;
        const nri::Result result = nri::video::av1::GetEncodeDecodeInfoFromHeader(desc, prefixInfo);
        REQUIRE(result != nri::Result::SUCCESS);
        REQUIRE(std::memcmp(&prefixInfo, &original, sizeof(prefixInfo)) == 0); // including "savedFrameState"
    }
}

TEST_CASE("VID-PAR-012 AV1 CodedLossless includes the ALT_Q of every segment", "[video][parser][short]") {
    const std::array<nri::VideoAV1ReferenceDesc, 8> references = MakeReferences();
    nri::VideoAV1EncodeDecodeInfoDesc base = {};
    base.references = references.data();
    base.referenceNum = 8;

    // base_q_idx = 0, but segment 1 has ALT_Q = +20: not coded lossless, loop filter and "tx_mode_select" are coded
    InterHeader header;
    header.baseQIndex = 0;
    header.segmentation = true;
    header.segment1AltQ = 20;

    nri::VideoAV1EncodeDecodeInfo info = {};
    REQUIRE(ParseInterHeader(header, &base, nullptr, info) == nri::Result::SUCCESS);
    REQUIRE(info.picture.baseQIndex == 0);
    REQUIRE(info.loopFilter.level[0] == 8);
    REQUIRE(info.loopFilter.level[2] == 4);
    REQUIRE(info.picture.txMode == 2); // TX_MODE_SELECT
    REQUIRE((info.picture.flags & nri::VideoAV1PictureBits::REDUCED_TX_SET));
}

TEST_CASE("VID-PAR-013 AV1 error resilient frames don't code use_ref_frame_mvs and allow_warped_motion", "[video][parser][short]") {
    const std::array<nri::VideoAV1ReferenceDesc, 8> references = MakeReferences();
    nri::VideoAV1EncodeDecodeInfoDesc base = {};
    base.references = references.data();
    base.referenceNum = 8;

    InterHeader header;
    header.enableRefFrameMvs = true;
    header.enableWarpedMotion = true;
    nri::VideoAV1EncodeDecodeInfo info = {};

    SECTION("error resilient") {
        header.errorResilient = true;
        REQUIRE(ParseInterHeader(header, &base, nullptr, info) == nri::Result::SUCCESS);
        REQUIRE(!(info.picture.flags & nri::VideoAV1PictureBits::USE_REF_FRAME_MVS));
        REQUIRE(!(info.picture.flags & nri::VideoAV1PictureBits::ALLOW_WARPED_MOTION));
        REQUIRE((info.picture.flags & nri::VideoAV1PictureBits::REDUCED_TX_SET));
    }
    SECTION("not error resilient") {
        REQUIRE(ParseInterHeader(header, &base, nullptr, info) == nri::Result::SUCCESS);
        REQUIRE(!(info.picture.flags & nri::VideoAV1PictureBits::ALLOW_WARPED_MOTION));
        REQUIRE((info.picture.flags & nri::VideoAV1PictureBits::REDUCED_TX_SET));
    }
}

TEST_CASE("VID-PAR-014 AV1 global motion models with an invalid shear are marked invalid", "[video][parser][short]") {
    const std::array<nri::VideoAV1ReferenceDesc, 8> references = MakeReferences();
    std::array<nri::VideoAV1SavedFrameState, 8> states = MakeSavedFrameStates();
    nri::VideoAV1EncodeDecodeInfoDesc base = {};
    base.references = references.data();
    base.referenceNum = 8;

    // LAST of the primary reference: ROTZOOM with alpha = beta = 8192 after "setup_shear", 4 * 8192 + 7 * 8192 >= 65536
    constexpr int32_t invalidModel[6] = {0, 0, 73728, 8192, -8192, 73728};
    for (uint32_t i = 0; i < 6; i++)
        states[0].globalMotion.params[1][i] = invalidModel[i];

    InterHeader header;
    header.globalRotZoomLast = true;
    header.primaryRefFrame = 0;

    nri::VideoAV1EncodeDecodeInfo info = {};
    REQUIRE(ParseInterHeader(header, &base, states.data(), info) == nri::Result::SUCCESS);
    REQUIRE(info.globalMotion.type[1] == 2);
    REQUIRE(info.globalMotion.params[1][2] == 73728);
    REQUIRE(info.globalMotion.invalid[1] == 1);
    REQUIRE(info.globalMotion.invalid[2] == 0);
    REQUIRE(info.savedFrameState.globalMotion.invalid[1] == 1);
}

TEST_CASE("VID-PAR-015 AV1 inter frames return the complete reference frame map", "[video][parser][short]") {
    const std::array<nri::VideoAV1ReferenceDesc, 8> references = MakeReferences();
    const std::array<nri::VideoAV1SavedFrameState, 8> states = MakeSavedFrameStates();
    nri::VideoAV1EncodeDecodeInfoDesc base = {};
    base.references = references.data();
    base.referenceNum = 8;

    // Short signaling: LAST = 7, GOLDEN = 0, the others 6, 5, 4, 3, 2; map entry 1 is not used by the frame
    InterHeader header;
    header.shortSignaling = true;
    header.lastFrameIdx = 7;
    header.goldFrameIdx = 0;

    nri::VideoAV1EncodeDecodeInfo info = {};
    REQUIRE(ParseInterHeader(header, &base, states.data(), info) == nri::Result::SUCCESS);
    REQUIRE(info.picture.referenceNum == 8);
    REQUIRE(info.picture.references[7].name == nri::VideoAV1ReferenceName::NONE);
    REQUIRE(info.picture.references[7].refFrameIndex == 1);
    REQUIRE(info.picture.references[7].slot == references[1].slot);
    REQUIRE(info.picture.references[7].orderHint == references[1].orderHint);

    // Fewer map entries: only the provided ones are returned
    base.referenceNum = 7; // entry 7 (LAST) is missing
    REQUIRE(ParseInterHeader(header, &base, states.data(), info) == nri::Result::FAILURE);
}
