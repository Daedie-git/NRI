// Copyright (c) 2026 NVIDIA Corporation

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <type_traits>

#include "NRI.h"

#include "Extensions/NRIVideo.h"

TEST_CASE("VID-API-002 video public structures support the C++ facade", "[video][api][short]") {
    STATIC_REQUIRE(std::is_standard_layout_v<nri::VideoSessionDesc>);
    STATIC_REQUIRE(std::is_standard_layout_v<nri::VideoCapabilities>);
    STATIC_REQUIRE(std::is_standard_layout_v<nri::VideoAV1EncodeDecodeInfo>);
    STATIC_REQUIRE(std::is_standard_layout_v<nri::VideoInterface>);
}

TEST_CASE("VID-API-003 video flag declarations are single non-overlapping bits", "[video][api][short]") {
    const uint32_t flags[] = {
        (uint32_t)nri::VideoAV1EncodeFeatureBits::ORDER_HINT_TOOLS,
        (uint32_t)nri::VideoAV1EncodeFeatureBits::LOOP_RESTORATION_FILTER,
        (uint32_t)nri::VideoAV1EncodeFeatureBits::FORCED_INTEGER_MOTION_VECTORS,
        (uint32_t)nri::VideoAV1EncodeFeatureBits::AUTO_SEGMENTATION,
        (uint32_t)nri::VideoAV1EncodeFeatureBits::CDEF_FILTERING,
        (uint32_t)nri::VideoAV1EncodeFeatureBits::QUANTIZATION_DELTAS,
        (uint32_t)nri::VideoAV1EncodeFeatureBits::LOOP_FILTER_DELTAS,
    };

    uint32_t combined = 0;

    for (uint32_t flag : flags) {
        REQUIRE(flag != 0);
        REQUIRE((flag & (flag - 1)) == 0);
        REQUIRE((combined & flag) == 0);
        combined |= flag;
    }
}
