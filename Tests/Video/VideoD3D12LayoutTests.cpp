// Copyright (c) 2026 NVIDIA Corporation

#include <catch2/catch_test_macros.hpp>

#include <type_traits>

#include "SharedD3D12.h"

TEST_CASE("VID-REG-024 D3D12 AV1 metadata uses the Agility SDK types", "[video][d3d12][regression][short]") {
    STATIC_REQUIRE(std::is_same<nri::VideoEncodeAV1TilesLayoutD3D12, D3D12_VIDEO_ENCODER_AV1_PICTURE_CONTROL_SUBREGIONS_LAYOUT_DATA_TILES>::value);
    STATIC_REQUIRE(std::is_same<nri::VideoEncodeAV1PostEncodeValuesD3D12, D3D12_VIDEO_ENCODER_AV1_POST_ENCODE_VALUES>::value);
}
