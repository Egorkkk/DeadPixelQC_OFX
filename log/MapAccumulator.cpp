#include "MapAccumulator.h"

namespace DeadPixelQC {

void MapAccumulator::reset() {
    framesIngested_ = 0;
}

void MapAccumulator::ingestFrame(i32 frameIndex, const DetectionHitList& hits) {
    // Stub only: map accumulation logic will be implemented in a follow-up.
    (void)frameIndex;
    (void)hits;
    ++framesIngested_;
}

DefectMap MapAccumulator::finalize(i32 width, i32 height, const char* sourceClip) const {
    DefectMap map;
    map.metadata().width = width;
    map.metadata().height = height;
    map.metadata().framesAnalyzed = framesIngested_;
    if (sourceClip != nullptr) {
        map.metadata().sourceClip = sourceClip;
    }
    return map;
}

} // namespace DeadPixelQC
