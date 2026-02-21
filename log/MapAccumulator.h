#pragma once

#include "../model/DataTypes.h"
#include "../model/DefectMap.h"

#include <cstdint>
#include <unordered_map>

namespace DeadPixelQC {

class MapAccumulator {
public:
    void reset();
    void ingestFrame(const DetectionHitList& hits, i32 width, i32 height);
    DefectMap finalize(i32 minHitCount) const;

private:
    static std::uint64_t makeKey(i32 x, i32 y);
    static void decodeKey(std::uint64_t key, i32& x, i32& y);

private:
    std::unordered_map<std::uint64_t, i32> hitCounts_;
    i32 sourceWidth_ = 0;
    i32 sourceHeight_ = 0;
    i32 framesIngested_ = 0;
};

} // namespace DeadPixelQC
