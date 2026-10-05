// Copyright (c) 2026 NVIDIA Corporation

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <vector>

#include "NRI.h"

#include "Extensions/NRIVideo.h"
#include "VideoAnnexB.h"

namespace {

nri::VideoH264SequenceParameterSetDesc MakeH264Sps() {
    nri::VideoH264SequenceParameterSetDesc desc = {};
    desc.flags = nri::VideoH264SequenceParameterSetBits::FRAME_MBS_ONLY | nri::VideoH264SequenceParameterSetBits::DIRECT_8X8_INFERENCE;
    desc.profileIdc = 100;
    desc.levelIdc = 40;
    desc.chromaFormatIdc = 1;
    desc.log2MaxFrameNumMinus4 = 4;
    desc.log2MaxPictureOrderCountLsbMinus4 = 4;
    desc.referenceFrameNum = 4;
    desc.pictureWidthInMbsMinus1 = 119;
    desc.pictureHeightInMapUnitsMinus1 = 67;

    return desc;
}

nri::VideoH264PictureParameterSetDesc MakeH264Pps() {
    nri::VideoH264PictureParameterSetDesc desc = {};
    desc.flags = nri::VideoH264PictureParameterSetBits::ENTROPY_CODING_MODE | nri::VideoH264PictureParameterSetBits::DEBLOCKING_FILTER_CONTROL_PRESENT;
    desc.refIndexL0DefaultActiveMinus1 = 1;
    desc.refIndexL1DefaultActiveMinus1 = 1;
    desc.chromaQpIndexOffset = -2;
    desc.secondChromaQpIndexOffset = -2;

    return desc;
}

std::vector<uint8_t> WriteH264(const nri::VideoH264SequenceParameterSetDesc& sps, const nri::VideoH264PictureParameterSetDesc& pps) {
    nri::VideoAnnexBParameterSetsDesc desc = {};
    desc.codec = nri::VideoCodec::H264;
    desc.h264Sps = &sps;
    desc.h264Pps = &pps;
    REQUIRE(nri::WriteVideoAnnexBParameterSetsShared(desc) == nri::Result::SUCCESS);
    REQUIRE(desc.writtenSize != 0);

    std::vector<uint8_t> bytes(desc.writtenSize);
    desc.dst = bytes.data();
    desc.dstSize = bytes.size();
    REQUIRE(nri::WriteVideoAnnexBParameterSetsShared(desc) == nri::Result::SUCCESS);
    REQUIRE(desc.writtenSize == bytes.size());

    return bytes;
}

nri::VideoH265VideoParameterSetDesc MakeH265Vps() {
    nri::VideoH265VideoParameterSetDesc desc = {};
    desc.flags = nri::VideoH265VideoParameterSetBits::TEMPORAL_ID_NESTING | nri::VideoH265VideoParameterSetBits::SUB_LAYER_ORDERING_INFO_PRESENT;
    desc.profileTierLevel.generalProfileIdc = 1;
    desc.profileTierLevel.generalLevelIdc = 120;

    return desc;
}

nri::VideoH265SequenceParameterSetDesc MakeH265Sps() {
    nri::VideoH265SequenceParameterSetDesc desc = {};
    desc.flags = nri::VideoH265SequenceParameterSetBits::TEMPORAL_ID_NESTING | nri::VideoH265SequenceParameterSetBits::SUB_LAYER_ORDERING_INFO_PRESENT;
    desc.chromaFormatIdc = 1;
    desc.pictureWidthInLumaSamples = 1920;
    desc.pictureHeightInLumaSamples = 1080;
    desc.log2MaxPictureOrderCountLsbMinus4 = 4;
    desc.log2MinLumaCodingBlockSizeMinus3 = 0;
    desc.log2DiffMaxMinLumaCodingBlockSize = 3;
    desc.log2MinLumaTransformBlockSizeMinus2 = 0;
    desc.log2DiffMaxMinLumaTransformBlockSize = 3;
    desc.profileTierLevel.generalProfileIdc = 1;
    desc.profileTierLevel.generalLevelIdc = 120;

    return desc;
}

nri::VideoH265PictureParameterSetDesc MakeH265Pps() {
    nri::VideoH265PictureParameterSetDesc desc = {};
    desc.flags = nri::VideoH265PictureParameterSetBits::DEBLOCKING_FILTER_CONTROL_PRESENT;
    desc.log2ParallelMergeLevelMinus2 = 0;

    return desc;
}

std::vector<uint8_t> WriteH265(const nri::VideoH265VideoParameterSetDesc& vps, const nri::VideoH265SequenceParameterSetDesc& sps, const nri::VideoH265PictureParameterSetDesc& pps) {
    nri::VideoAnnexBParameterSetsDesc desc = {};
    desc.codec = nri::VideoCodec::H265;
    desc.h265Vps = &vps;
    desc.h265Sps = &sps;
    desc.h265Pps = &pps;
    REQUIRE(nri::WriteVideoAnnexBParameterSetsShared(desc) == nri::Result::SUCCESS);

    std::vector<uint8_t> bytes(desc.writtenSize);
    desc.dst = bytes.data();
    desc.dstSize = bytes.size();
    REQUIRE(nri::WriteVideoAnnexBParameterSetsShared(desc) == nri::Result::SUCCESS);

    return bytes;
}

} // namespace

TEST_CASE("VID-SER-001 Annex-B size queries and bounds are exact", "[video][serializer][short]") {
    const nri::VideoH264SequenceParameterSetDesc sps = MakeH264Sps();
    const nri::VideoH264PictureParameterSetDesc pps = MakeH264Pps();
    const std::vector<uint8_t> expected = WriteH264(sps, pps);

    std::vector<uint8_t> guarded(expected.size() + 2, 0xA5);
    nri::VideoAnnexBParameterSetsDesc desc = {};
    desc.codec = nri::VideoCodec::H264;
    desc.h264Sps = &sps;
    desc.h264Pps = &pps;
    desc.dst = guarded.data() + 1;
    desc.dstSize = expected.size() - 1;
    REQUIRE(nri::WriteVideoAnnexBParameterSetsShared(desc) == nri::Result::INVALID_ARGUMENT);
    REQUIRE(guarded.front() == 0xA5);
    REQUIRE(guarded.back() == 0xA5);

    desc.dstSize = expected.size();
    REQUIRE(nri::WriteVideoAnnexBParameterSetsShared(desc) == nri::Result::SUCCESS);
    REQUIRE(std::equal(expected.begin(), expected.end(), guarded.begin() + 1));
    REQUIRE(guarded.front() == 0xA5);
    REQUIRE(guarded.back() == 0xA5);
}

TEST_CASE("VID-REG-004 H.264 represented syntax changes the bytestream", "[video][serializer][regression][short]") {
    nri::VideoH264SequenceParameterSetDesc sps = MakeH264Sps();
    const nri::VideoH264PictureParameterSetDesc pps = MakeH264Pps();

    const std::vector<uint8_t> pocType0 = WriteH264(sps, pps);
    sps.pictureOrderCountType = 1;
    sps.offsetForNonReferencePicture = -3;
    sps.offsetForTopToBottomField = 5;
    const std::vector<uint8_t> pocType1 = WriteH264(sps, pps);

    sps.offsetForNonReferencePicture = -4;
    const std::vector<uint8_t> changedPocOffset = WriteH264(sps, pps);
    REQUIRE(pocType0 != pocType1);
    REQUIRE(pocType1 != changedPocOffset);

    nri::VideoH264PictureParameterSetDesc changedPps = pps;
    changedPps.secondChromaQpIndexOffset = 3;
    REQUIRE(WriteH264(sps, pps) != WriteH264(sps, changedPps));
}

TEST_CASE("VID-REG-005 H.265 represented syntax is encoded or rejected", "[video][serializer][regression][short]") {
    nri::VideoH265VideoParameterSetDesc vps = MakeH265Vps();
    nri::VideoH265SequenceParameterSetDesc sps = MakeH265Sps();
    const nri::VideoH265PictureParameterSetDesc pps = MakeH265Pps();
    const std::vector<uint8_t> withoutScalingList = WriteH265(vps, sps, pps);

    nri::VideoH265ScalingListsDesc scalingLists = {};
    sps.flags |= nri::VideoH265SequenceParameterSetBits::SCALING_LIST_ENABLED | nri::VideoH265SequenceParameterSetBits::SCALING_LIST_DATA_PRESENT;
    sps.scalingLists = &scalingLists;

    nri::VideoAnnexBParameterSetsDesc desc = {};
    desc.codec = nri::VideoCodec::H265;
    desc.h265Vps = &vps;
    desc.h265Sps = &sps;
    desc.h265Pps = &pps;
    const nri::Result result = nri::WriteVideoAnnexBParameterSetsShared(desc);
    REQUIRE((result == nri::Result::SUCCESS || result == nri::Result::UNSUPPORTED));

    if (result == nri::Result::SUCCESS)
        REQUIRE(WriteH265(vps, sps, pps) != withoutScalingList);

    sps = MakeH265Sps();
    nri::VideoAnnexBParameterSetsDesc invalidSubLayers = {};
    invalidSubLayers.codec = nri::VideoCodec::H265;
    invalidSubLayers.h265Vps = &vps;
    invalidSubLayers.h265Sps = &sps;
    invalidSubLayers.h265Pps = &pps;

    vps.maxSubLayersMinus1 = 7;
    CHECK(nri::WriteVideoAnnexBParameterSetsShared(invalidSubLayers) == nri::Result::INVALID_ARGUMENT);

    vps = MakeH265Vps();
    sps.maxSubLayersMinus1 = 7;
    CHECK(nri::WriteVideoAnnexBParameterSetsShared(invalidSubLayers) == nri::Result::INVALID_ARGUMENT);
}

TEST_CASE("VID-SER-003 Annex-B end markers have exact transactional writes", "[video][serializer][short]") {
    for (nri::VideoCodec codec : {nri::VideoCodec::H264, nri::VideoCodec::H265}) {
        nri::VideoAnnexBEndOfStreamDesc desc = {};
        desc.codec = codec;
        REQUIRE(nri::WriteVideoAnnexBEndOfStreamShared(desc) == nri::Result::SUCCESS);
        REQUIRE(desc.writtenSize != 0);

        std::vector<uint8_t> bytes(desc.writtenSize + 2, 0xA5);
        desc.dst = bytes.data() + 1;
        desc.dstSize = desc.writtenSize - 1;
        REQUIRE(nri::WriteVideoAnnexBEndOfStreamShared(desc) == nri::Result::INVALID_ARGUMENT);
        REQUIRE(std::all_of(bytes.begin(), bytes.end(), [](uint8_t value) { return value == 0xA5; }));

        desc.dstSize++;
        REQUIRE(nri::WriteVideoAnnexBEndOfStreamShared(desc) == nri::Result::SUCCESS);
        REQUIRE(bytes.front() == 0xA5);
        REQUIRE(bytes.back() == 0xA5);
    }

    nri::VideoAnnexBEndOfStreamDesc invalid = {};
    invalid.codec = (nri::VideoCodec)UINT8_MAX;
    invalid.writtenSize = 0xA5A5;
    REQUIRE(nri::WriteVideoAnnexBEndOfStreamShared(invalid) == nri::Result::UNSUPPORTED);
    REQUIRE(invalid.writtenSize == 0xA5A5);
}

TEST_CASE("VID-SER-004 RBSP integer extremes and emulation prevention are lossless", "[video][serializer][short]") {
    nri::video_annex_b::ByteWriter bytes = {};
    nri::video_annex_b::RbspBitWriter writer = {bytes};
    writer.WriteUe(UINT32_MAX);
    writer.WriteSe(INT32_MIN);
    writer.WriteSe(INT32_MAX);
    writer.FinishRbsp();
    REQUIRE_FALSE(bytes.overflow);
    REQUIRE(bytes.writtenSize != 0);

    std::array<uint8_t, 8> output = {};
    nri::video_annex_b::ByteWriter escapedBytes = {output.data(), output.size()};
    nri::video_annex_b::RbspBitWriter escapedWriter = {escapedBytes};
    escapedWriter.WriteRbspByte(0);
    escapedWriter.WriteRbspByte(0);
    escapedWriter.WriteRbspByte(1);
    REQUIRE(escapedBytes.writtenSize == 4);
    REQUIRE(output[0] == 0);
    REQUIRE(output[1] == 0);
    REQUIRE(output[2] == 3);
    REQUIRE(output[3] == 1);
}

TEST_CASE("VID-SER-005 unsupported parameter sets fail before writing", "[video][serializer][short]") {
    nri::VideoH264SequenceParameterSetDesc h264Sps = MakeH264Sps();
    nri::VideoH264PictureParameterSetDesc h264Pps = MakeH264Pps();
    std::array<uint8_t, 32> dst = {};
    dst.fill(0xA5);

    nri::VideoAnnexBParameterSetsDesc h264 = {};
    h264.codec = nri::VideoCodec::H264;
    h264.h264Sps = &h264Sps;
    h264.h264Pps = &h264Pps;
    h264.dst = dst.data();
    h264.dstSize = dst.size();
    h264.writtenSize = 0x11223344;

    h264Sps.pictureOrderCountType = 3;
    REQUIRE(nri::WriteVideoAnnexBParameterSetsShared(h264) == nri::Result::UNSUPPORTED);
    REQUIRE(h264.writtenSize == 0x11223344);
    REQUIRE(std::all_of(dst.begin(), dst.end(), [](uint8_t value) { return value == 0xA5; }));

    nri::VideoH265VideoParameterSetDesc h265Vps = MakeH265Vps();
    nri::VideoH265SequenceParameterSetDesc h265Sps = MakeH265Sps();
    nri::VideoH265PictureParameterSetDesc h265Pps = MakeH265Pps();
    h265Sps.numShortTermRefPicSets = 1;

    nri::VideoAnnexBParameterSetsDesc h265 = {};
    h265.codec = nri::VideoCodec::H265;
    h265.h265Vps = &h265Vps;
    h265.h265Sps = &h265Sps;
    h265.h265Pps = &h265Pps;
    h265.dst = dst.data();
    h265.dstSize = dst.size();
    h265.writtenSize = 0x55667788;
    REQUIRE(nri::WriteVideoAnnexBParameterSetsShared(h265) == nri::Result::UNSUPPORTED);
    REQUIRE(h265.writtenSize == 0x55667788);
    REQUIRE(std::all_of(dst.begin(), dst.end(), [](uint8_t value) { return value == 0xA5; }));
}

TEST_CASE("VID-SER-007 Annex-B representative encodings match golden bytes", "[video][serializer][short]") {
    const std::array<uint8_t, 24> h264Golden = {
        0x00,
        0x00,
        0x00,
        0x01,
        0x67,
        0x64,
        0x00,
        0x28,
        0xAC,
        0x2C,
        0xA5,
        0x01,
        0xE0,
        0x08,
        0x99,
        0x00,
        0x00,
        0x00,
        0x01,
        0x68,
        0xEA,
        0x43,
        0x2C,
        0x80,
    };
    REQUIRE(WriteH264(MakeH264Sps(), MakeH264Pps()) == std::vector<uint8_t>(h264Golden.begin(), h264Golden.end()));

    const std::array<uint8_t, 73> h265Golden = {
        0x00,
        0x00,
        0x00,
        0x01,
        0x40,
        0x01,
        0x0C,
        0x01,
        0xFF,
        0xFF,
        0x01,
        0x40,
        0x00,
        0x00,
        0x03,
        0x00,
        0x00,
        0x03,
        0x00,
        0x00,
        0x03,
        0x00,
        0x00,
        0x03,
        0x00,
        0x78,
        0xF0,
        0x24,
        0x00,
        0x00,
        0x00,
        0x01,
        0x42,
        0x01,
        0x01,
        0x01,
        0x40,
        0x00,
        0x00,
        0x03,
        0x00,
        0x00,
        0x03,
        0x00,
        0x00,
        0x03,
        0x00,
        0x00,
        0x03,
        0x00,
        0x78,
        0xA0,
        0x03,
        0xC0,
        0x80,
        0x10,
        0xE5,
        0x97,
        0xE4,
        0x93,
        0x08,
        0x20,
        0x00,
        0x00,
        0x00,
        0x01,
        0x44,
        0x01,
        0xC0,
        0x71,
        0x80,
        0x99,
        0x20,
    };
    REQUIRE(WriteH265(MakeH265Vps(), MakeH265Sps(), MakeH265Pps()) == std::vector<uint8_t>(h265Golden.begin(), h265Golden.end()));
}
