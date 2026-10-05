// Copyright (c) 2026 NVIDIA Corporation

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <cstdlib>

#include "NRI.h"

#include "Extensions/NRIVideo.h"
#include "VideoAnnexB.h"

TEST_CASE("VID-SOAK-001 serializer soak", "[video][soak]") {
    nri::VideoAnnexBEndOfStreamDesc desc = {};
    desc.codec = nri::VideoCodec::H264;
    uint32_t durationMs = 10 * 60 * 1000;

    if (const char* overrideValue = std::getenv("NRI_VIDEO_TEST_DURATION_MS"))
        durationMs = (uint32_t)std::strtoul(overrideValue, nullptr, 10);

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(durationMs);
    uint64_t iterationNum = 0;

    while (std::chrono::steady_clock::now() < deadline) {
        REQUIRE(nri::WriteVideoAnnexBEndOfStreamShared(desc) == nri::Result::SUCCESS);
        desc.codec = desc.codec == nri::VideoCodec::H264 ? nri::VideoCodec::H265 : nri::VideoCodec::H264;
        iterationNum++;
    }

    REQUIRE(iterationNum != 0);
}
