#pragma once

#include "NRI.h"

#include <limits>

namespace nri {

constexpr float GetRenderPassDepthClearValue(const AttachmentDesc* depth, bool readOnly) noexcept {
    if (!depth || readOnly)
        return std::numeric_limits<float>::quiet_NaN();
    return depth->loadOp == LoadOp::CLEAR ? depth->clearValue.depthStencil.depth : 0.0f;
}

} // namespace nri
