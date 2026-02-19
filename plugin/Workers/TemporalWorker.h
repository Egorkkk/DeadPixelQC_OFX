#pragma once

#include "SpatialWorker.h"

namespace DeadPixelQC_OFX::Workers {

struct TemporalParams {
    bool enable = false;
    int windowFrames = 60;
};

struct TemporalResult {
    bool ok = true;
};

struct TemporalWorker {
    TemporalResult process(const FrameView& in, FrameView& out, const TemporalParams& p);
};

} // namespace DeadPixelQC_OFX::Workers
