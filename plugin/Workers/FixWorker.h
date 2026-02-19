#pragma once

#include "SpatialWorker.h"

namespace DeadPixelQC_OFX::Workers {

struct FixParams {
    bool enable = false;
    int method = 0;
};

struct FixResult {
    bool ok = true;
};

struct FixWorker {
    FixResult process(const FrameView& in, FrameView& out, const FixParams& p);
};

} // namespace DeadPixelQC_OFX::Workers
