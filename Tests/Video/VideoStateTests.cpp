// Copyright (c) 2026 NVIDIA Corporation

#include <catch2/catch_test_macros.hpp>

#include "NRI.h"

#include "Extensions/NRIVideo.h"
#include "VideoShared.h"

TEST_CASE("VID-REG-008 H.264 setup slot override requires hasReferenceSlot", "[video][state][regression][short]") {
    nri::VideoH264DecodePictureDesc picture = {};
    picture.hasReferenceSlot = true;
    picture.referenceSlot = 0;

    nri::VideoDecodeDesc desc = {};
    desc.dstSlot = 5;
    desc.h264PictureDesc = &picture;
    REQUIRE(nri::GetVideoDecodeSetupSlot(desc) == 0);

    picture.referenceSlot = 3;
    picture.hasReferenceSlot = false;
    REQUIRE(nri::GetVideoDecodeSetupSlot(desc) == 5);

    picture.referenceSlot = 0;
    picture.hasReferenceSlot = false;
    REQUIRE(nri::GetVideoDecodeSetupSlot(desc) == 5);
}
