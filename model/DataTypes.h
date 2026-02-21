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
    bool confirmed = false;
    i32 persistence = 0;
    DetectionHitList hits;
};

struct DefectPointNorm {
    f32 u = 0.0f;
    f32 v = 0.0f;
    f32 weight = 1.0f;
};

struct DefectMapMetadata {
    i32 version = 1;
    i32 sourceWidth = 0;
    i32 sourceHeight = 0;
    i32 framesAnalyzed = 0;
    std::string createdUtc;
    std::string cameraTag;
};

} // namespace DeadPixelQC
