#pragma once

#include "NRI.h"

namespace nri {

constexpr float GetRenderPassDepthClearValue(const AttachmentDesc* depth, bool readOnly) noexcept {
    if (!depth || readOnly)
        return 0.0f;
    return depth->loadOp == LoadOp::CLEAR ? depth->clearValue.depthStencil.depth : 0.0f;
}

} // namespace nri
