// Copyright (c) 2026 NVIDIA Corporation

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstdlib>

#include "NRI.h"

#include "Extensions/NRIVideo.h"
#include "SharedExternal.h"

TEST_CASE("VID-STRESS-001 serializers remain stable across repeated calls", "[video][stress]") {
    uint32_t iterationNum = 1000000;

    if (const char* overrideValue = std::getenv("NRI_VIDEO_TEST_ITERATIONS"))
        iterationNum = (uint32_t)std::strtoul(overrideValue, nullptr, 10);

    nri::VideoAV1ObuHeadersDesc desc = {};
    desc.sequence.flags = nri::VideoAV1SequenceBits::ENABLE_ORDER_HINT;
    desc.sequence.bitDepth = 8;
    desc.sequence.subsamplingX = 1;
    desc.sequence.subsamplingY = 1;
    desc.sequence.maxFrameWidthMinus1 = 1919;
    desc.sequence.maxFrameHeightMinus1 = 1079;
    desc.sequence.frameWidthBitsMinus1 = 10;
    desc.sequence.frameHeightBitsMinus1 = 10;
    desc.sequence.orderHintBitsMinus1 = 7;

    std::array<uint8_t, 256> output = {};
    desc.dst = output.data();
    desc.dstSize = output.size();
    REQUIRE(nri::video::WriteAV1ObuHeaders(desc) == nri::Result::SUCCESS);
    const uint64_t expectedSize = desc.writtenSize;
    const std::array<uint8_t, 256> expected = output;

    for (uint32_t i = 0; i < iterationNum; i++) {
        output.fill(0);
        REQUIRE(nri::video::WriteAV1ObuHeaders(desc) == nri::Result::SUCCESS);
        REQUIRE(desc.writtenSize == expectedSize);
        REQUIRE(output == expected);
    }
}
