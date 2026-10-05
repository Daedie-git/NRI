// Copyright (c) 2026 NVIDIA Corporation

#include "NRI.h"

#include "Extensions/NRIVideo.h"

// VID-API-001: the complete public video facade must compile as C11.

_Static_assert(sizeof(NriVideoSessionType) == sizeof(uint8_t), "VideoSessionType ABI changed");
_Static_assert(sizeof(NriVideoEncodeFrameType) == sizeof(uint8_t), "VideoEncodeFrameType ABI changed");
_Static_assert(sizeof(NriVideoInterface) % sizeof(void*) == 0, "VideoInterface must contain only complete function slots");

void nriVideoCHeaderTest(void) {
    NriVideoSessionDesc desc = {0};
    desc.type = NriVideoSessionType_DECODE;
    desc.codec = NriVideoCodec_H264;
}
