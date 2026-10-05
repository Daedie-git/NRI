// Copyright (c) 2026 NVIDIA Corporation

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstdint>
#include <cstdlib>

#include "NRI.h"

#include "Extensions/NRIVideo.h"
#include "VideoAnnexB.h"

TEST_CASE("VID-FUZZ-001 deterministic AV1 sequence descriptor fuzz", "[video][fuzz]") {
    uint32_t iterationNum = 250000;

    if (const char* overrideValue = std::getenv("NRI_VIDEO_TEST_ITERATIONS"))
        iterationNum = (uint32_t)std::strtoul(overrideValue, nullptr, 10);

    uint32_t state = 0xC001D00Du;
    auto random = [&state]() {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;

        return state;
    };

    for (uint32_t i = 0; i < iterationNum; i++) {
        nri::VideoAV1ObuHeadersDesc desc = {};
        uint8_t* sequenceBytes = (uint8_t*)&desc.sequence;

        for (size_t j = 0; j < sizeof(desc.sequence); j++)
            sequenceBytes[j] = (uint8_t)random();

        std::array<uint8_t, 514> guarded = {};
        guarded.fill(0xA5);
        desc.dst = guarded.data() + 1;
        desc.dstSize = guarded.size() - 2;
        const nri::Result result = nri::WriteVideoAV1ObuHeadersShared(desc);
        REQUIRE((result == nri::Result::SUCCESS || result == nri::Result::INVALID_ARGUMENT));
        REQUIRE(guarded.front() == 0xA5);
        REQUIRE(guarded.back() == 0xA5);

        if (result == nri::Result::SUCCESS)
            REQUIRE(desc.writtenSize <= desc.dstSize);
    }
}
