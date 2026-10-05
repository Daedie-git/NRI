// Copyright (c) 2026 NVIDIA Corporation

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstdint>
#include <cstring>
#include <vector>

#include "NRI.h"

#include "Extensions/NRIVideo.h"
#include "VideoAV1.h"
#include "VideoAnnexB.h"

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
    obu.push_back((uint8_t(nri::video_av1::ObuType::Frame) << 3u) | 0x02u);
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

std::vector<uint8_t> MakeInterFramePayload(uint8_t baseQIndex = 32) {
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
        references[i].frameType = i == 0 ? nri::VideoEncodeFrameType::IDR : nri::VideoEncodeFrameType::P;
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

} // namespace

TEST_CASE("VID-SER-002 AV1 OBU size queries and bounds are exact", "[video][serializer][short]") {
    nri::VideoAV1ObuHeadersDesc desc = {};
    desc.sequence = MakeSequence();
    REQUIRE(nri::WriteVideoAV1ObuHeadersShared(desc) == nri::Result::SUCCESS);
    REQUIRE(desc.writtenSize != 0);

    std::vector<uint8_t> bytes(desc.writtenSize + 2, 0xA5);
    desc.dst = bytes.data() + 1;
    desc.dstSize = desc.writtenSize - 1;
    REQUIRE(nri::WriteVideoAV1ObuHeadersShared(desc) == nri::Result::INVALID_ARGUMENT);
    REQUIRE(bytes.front() == 0xA5);
    REQUIRE(bytes.back() == 0xA5);

    desc.dstSize = desc.writtenSize;
    REQUIRE(nri::WriteVideoAV1ObuHeadersShared(desc) == nri::Result::SUCCESS);
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
    REQUIRE(nri::video_av1::GetVideoEncodeAV1DecodeInfoFromHeader(desc, info) == nri::Result::SUCCESS);
    REQUIRE(info.picture.frameType == nri::VideoEncodeFrameType::P);
    REQUIRE(info.picture.refreshFrameFlags == 0x24);
    REQUIRE(info.picture.primaryReferenceName == nri::VideoAV1ReferenceName::GOLDEN);
    REQUIRE(info.picture.referenceNum == 7);
    REQUIRE(info.picture.references[0].slot == 10);
    REQUIRE(info.bitstreamOffset <= payload.size());
    REQUIRE(info.bitstreamSize == payload.size() - info.bitstreamOffset);
    REQUIRE(info.picture.tileNum == 1);
    REQUIRE(info.picture.tiles);

    for (uint32_t i = 0; i < info.picture.tileNum; i++)
        REQUIRE(uint64_t(info.picture.tiles[i].offset) + info.picture.tiles[i].size <= info.bitstreamSize);
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
    REQUIRE(nri::video_av1::GetVideoEncodeAV1DecodeInfoFromHeader(desc, zeroed) == nri::Result::SUCCESS);
    REQUIRE(nri::video_av1::GetVideoEncodeAV1DecodeInfoFromHeader(desc, poisoned) == nri::Result::SUCCESS);

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
    REQUIRE(nri::WriteVideoAV1ObuHeadersShared(desc) == nri::Result::INVALID_ARGUMENT);

    desc.sequence = MakeSequence();
    desc.sequence.flags |= nri::VideoAV1SequenceBits::TIMING_INFO_PRESENT;
    desc.sequence.numTicksPerPictureMinus1 = UINT32_MAX;
    REQUIRE(nri::WriteVideoAV1ObuHeadersShared(desc) == nri::Result::INVALID_ARGUMENT);

    desc.sequence = MakeSequence();
    desc.sequence.seqProfile = 0;
    desc.sequence.bitDepth = 12;
    REQUIRE(nri::WriteVideoAV1ObuHeadersShared(desc) == nri::Result::INVALID_ARGUMENT);
}

TEST_CASE("VID-PAR-002 AV1 OBU scanner rejects cursor and extension hazards", "[video][parser][short]") {
    nri::video_av1::ObuSpan span = {};
    const std::array<uint8_t, 4> obu = {
        uint8_t((uint8_t(nri::video_av1::ObuType::Frame) << 3u) | 0x06u),
        0x01,
        0x01,
        0x00,
    };

    size_t cursor = obu.size();
    REQUIRE_FALSE(nri::video_av1::ReadObuHeader(obu.data(), obu.size(), cursor, span));

    cursor = obu.size() + 1;
    REQUIRE_FALSE(nri::video_av1::ReadObuHeader(obu.data(), obu.size(), cursor, span));

    cursor = 0;
    REQUIRE_FALSE(nri::video_av1::ReadObuHeader(obu.data(), obu.size(), cursor, span));

    std::array<uint8_t, 4> valid = obu;
    valid[1] = 0;
    cursor = 0;
    REQUIRE(nri::video_av1::ReadObuHeader(valid.data(), valid.size(), cursor, span));
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
        const nri::Result result = nri::video_av1::GetVideoEncodeAV1DecodeInfoFromHeader(desc, info);

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
        REQUIRE(nri::WriteVideoAV1ObuHeadersShared(desc) == nri::Result::INVALID_ARGUMENT);

        desc.sequence.flags |= nri::VideoAV1SequenceBits::STILL_PICTURE | nri::VideoAV1SequenceBits::ENABLE_ORDER_HINT;
        REQUIRE(nri::WriteVideoAV1ObuHeadersShared(desc) == nri::Result::INVALID_ARGUMENT);
    }

    SECTION("profile one color constraints") {
        desc.sequence.seqProfile = 1;
        desc.sequence.subsamplingX = 1;
        REQUIRE(nri::WriteVideoAV1ObuHeadersShared(desc) == nri::Result::INVALID_ARGUMENT);
    }

    SECTION("frame dimensions fit declared widths") {
        desc.sequence.frameWidthBitsMinus1 = 0;
        desc.sequence.maxFrameWidthMinus1 = 2;
        REQUIRE(nri::WriteVideoAV1ObuHeadersShared(desc) == nri::Result::INVALID_ARGUMENT);
    }

    SECTION("screen-content selectors") {
        desc.sequence.seqForceScreenContentTools = 3;
        REQUIRE(nri::WriteVideoAV1ObuHeadersShared(desc) == nri::Result::INVALID_ARGUMENT);
    }

    SECTION("monochrome inferred fields") {
        desc.sequence.flags |= nri::VideoAV1SequenceBits::MONO_CHROME | nri::VideoAV1SequenceBits::SEPARATE_UV_DELTA_Q;
        REQUIRE(nri::WriteVideoAV1ObuHeadersShared(desc) == nri::Result::INVALID_ARGUMENT);
    }

    SECTION("identity-matrix inferred fields") {
        desc.sequence.seqProfile = 1;
        desc.sequence.subsamplingX = 0;
        desc.sequence.subsamplingY = 0;
        desc.sequence.flags |= nri::VideoAV1SequenceBits::COLOR_DESCRIPTION_PRESENT;
        desc.sequence.colorPrimaries = 1;           // BT.709
        desc.sequence.transferCharacteristics = 13; // sRGB
        desc.sequence.matrixCoefficients = 0;       // identity
        REQUIRE(nri::WriteVideoAV1ObuHeadersShared(desc) == nri::Result::INVALID_ARGUMENT);
    }
}

TEST_CASE("VID-SER-008 AV1 representative OBU headers match golden bytes", "[video][serializer][short]") {
    nri::VideoAV1ObuHeadersDesc desc = {};
    desc.sequence = MakeSequence();
    REQUIRE(nri::WriteVideoAV1ObuHeadersShared(desc) == nri::Result::SUCCESS);

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
    REQUIRE(nri::WriteVideoAV1ObuHeadersShared(desc) == nri::Result::SUCCESS);
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
        REQUIRE(nri::WriteVideoAV1ObuHeadersShared(desc) == nri::Result::SUCCESS);
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
        REQUIRE(nri::WriteVideoAV1ObuHeadersShared(desc) == nri::Result::SUCCESS);
        REQUIRE(desc.writtenSize == golden.size());
        REQUIRE(bytes == golden);
    }
}
