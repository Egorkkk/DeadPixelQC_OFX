#pragma once

#include "../core/Types.h"
#include <string>
#include <vector>

namespace DeadPixelQC {

struct DetectionHit {
    i32 x = 0;
    i32 y = 0;
    f32 confidence = 1.0f;
};

using DetectionHitList = std::vector<DetectionHit>;

struct DetectionEvent {
    i32 frameIndex = -1;
    DetectionHitList hits;
};

struct DefectPointNorm {
    f32 u = 0.0f;
    f32 v = 0.0f;
};

struct DefectMapMetadata {
    i32 width = 0;
    i32 height = 0;
    i32 framesAnalyzed = 0;
    std::string sourceClip;
};

} // namespace DeadPixelQC
