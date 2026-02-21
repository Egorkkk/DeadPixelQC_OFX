#include "MapAccumulator.h"

#include <algorithm>

namespace DeadPixelQC {

void MapAccumulator::reset() {
    hitCounts_.clear();
    sourceWidth_ = 0;
    sourceHeight_ = 0;
    framesIngested_ = 0;
}

void MapAccumulator::ingestFrame(const DetectionHitList& hits, i32 width, i32 height) {
    if (width <= 0 || height <= 0) {
        return;
    }

    if (sourceWidth_ <= 0 || sourceHeight_ <= 0) {
        sourceWidth_ = width;
        sourceHeight_ = height;
    } else if (sourceWidth_ != width || sourceHeight_ != height) {
        // Restart accumulation if source resolution changes.
        reset();
        sourceWidth_ = width;
        sourceHeight_ = height;
    }

    for (const DetectionHit& hit : hits) {
        if (hit.x < 0 || hit.x >= width || hit.y < 0 || hit.y >= height) {
            continue;
        }
        ++hitCounts_[makeKey(hit.x, hit.y)];
    }

    ++framesIngested_;
}

DefectMap MapAccumulator::finalize(i32 minHitCount) const {
    DefectMap map;
    map.metadata().sourceWidth = sourceWidth_;
    map.metadata().sourceHeight = sourceHeight_;
    map.metadata().framesAnalyzed = framesIngested_;

    if (sourceWidth_ <= 0 || sourceHeight_ <= 0) {
        return map;
    }

    const i32 threshold = std::max<i32>(1, minHitCount);
    const f32 invWidth = (sourceWidth_ > 1) ? (1.0f / static_cast<f32>(sourceWidth_ - 1)) : 0.0f;
    const f32 invHeight = (sourceHeight_ > 1) ? (1.0f / static_cast<f32>(sourceHeight_ - 1)) : 0.0f;

    for (const auto& kv : hitCounts_) {
        const i32 hitCount = kv.second;
        if (hitCount < threshold) {
            continue;
        }

        i32 x = 0;
        i32 y = 0;
        decodeKey(kv.first, x, y);

        DefectPointNorm point;
        point.u = static_cast<f32>(x) * invWidth;
        point.v = static_cast<f32>(y) * invHeight;
        point.weight = static_cast<f32>(hitCount);
        map.addPoint(point);
    }

    auto& points = map.points();
    std::sort(points.begin(), points.end(), [](const DefectPointNorm& a, const DefectPointNorm& b) {
        if (a.v == b.v) {
            return a.u < b.u;
        }
        return a.v < b.v;
    });

    return map;
}

std::uint64_t MapAccumulator::makeKey(i32 x, i32 y) {
    const std::uint32_t ux = static_cast<std::uint32_t>(x);
    const std::uint32_t uy = static_cast<std::uint32_t>(y);
    return (static_cast<std::uint64_t>(ux) << 32) | static_cast<std::uint64_t>(uy);
}

void MapAccumulator::decodeKey(std::uint64_t key, i32& x, i32& y) {
    x = static_cast<i32>(static_cast<std::uint32_t>(key >> 32));
    y = static_cast<i32>(static_cast<std::uint32_t>(key & 0xFFFFFFFFULL));
}

} // namespace DeadPixelQC
