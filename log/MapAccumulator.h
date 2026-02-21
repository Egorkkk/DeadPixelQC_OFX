#pragma once

#include "../model/DataTypes.h"
#include "../model/DefectMap.h"

namespace DeadPixelQC {

class MapAccumulator {
public:
    void reset();
    void ingestFrame(i32 frameIndex, const DetectionHitList& hits);
    DefectMap finalize(i32 width, i32 height, const char* sourceClip = nullptr) const;

private:
    i32 framesIngested_ = 0;
};

} // namespace DeadPixelQC
