#include "EventLogger.h"

#include <algorithm>
#include <cmath>

namespace DeadPixelQC {

EventLogger::EventLogger(const Params& params)
    : params_(sanitizeParams(params)) {}

void EventLogger::setParams(const Params& params) {
    params_ = sanitizeParams(params);
}

const EventLogger::Params& EventLogger::params() const {
    return params_;
}

void EventLogger::ingestFrame(i32 frameIndex, const DetectionHitList& hits) {
    if (frameIndex < 0 || hits.empty()) {
        return;
    }

    for (const DetectionHit& hit : hits) {
        const std::uint64_t key = resolveTrackKey(hit.x, hit.y);
        TrackState& track = tracks_[key];

        if (track.lastSeenFrame < 0) {
            track.persistenceCount = 1;
        } else {
            const i32 frameDelta = frameIndex - track.lastSeenFrame;
            if (frameDelta > 0 && frameDelta <= (params_.maxGapFrames + 1)) {
                ++track.persistenceCount;
            } else {
                track.persistenceCount = 1;
            }
        }

        track.lastSeenFrame = frameIndex;

        if (track.persistenceCount >= params_.minPersistenceFrames &&
            frameIndex >= track.cooldownUntilFrame) {
            DetectionEvent event;
            event.frameIndex = frameIndex;
            event.confirmed = true;
            event.persistence = track.persistenceCount;
            event.hits.push_back(hit);
            model_.addEvent(event);

            track.cooldownUntilFrame = frameIndex + params_.cooldownFrames;
        }
    }
}

const EventModel& EventLogger::model() const {
    return model_;
}

EventModel& EventLogger::model() {
    return model_;
}

void EventLogger::clear() {
    model_.clear();
    tracks_.clear();
}

std::uint64_t EventLogger::makeKey(i32 x, i32 y) {
    const std::uint32_t ux = static_cast<std::uint32_t>(x);
    const std::uint32_t uy = static_cast<std::uint32_t>(y);
    return (static_cast<std::uint64_t>(ux) << 32) | static_cast<std::uint64_t>(uy);
}

std::uint64_t EventLogger::resolveTrackKey(i32 x, i32 y) const {
    if (params_.mergeRadiusPx <= 0 || tracks_.empty()) {
        return makeKey(x, y);
    }

    const i32 radius = params_.mergeRadiusPx;
    const i32 radiusSq = radius * radius;
    std::uint64_t bestKey = makeKey(x, y);
    i32 bestDistSq = radiusSq + 1;

    for (const auto& kv : tracks_) {
        const std::uint64_t key = kv.first;
        const i32 tx = static_cast<i32>(static_cast<std::uint32_t>(key >> 32));
        const i32 ty = static_cast<i32>(static_cast<std::uint32_t>(key & 0xFFFFFFFFULL));

        const i32 dx = tx - x;
        const i32 dy = ty - y;
        const i32 distSq = dx * dx + dy * dy;
        if (distSq <= radiusSq && distSq < bestDistSq) {
            bestDistSq = distSq;
            bestKey = key;
        }
    }

    return bestKey;
}

EventLogger::Params EventLogger::sanitizeParams(const Params& params) {
    Params out = params;
    out.minPersistenceFrames = std::max<i32>(1, out.minPersistenceFrames);
    out.maxGapFrames = std::max<i32>(0, out.maxGapFrames);
    out.cooldownFrames = std::max<i32>(0, out.cooldownFrames);
    out.mergeRadiusPx = std::max<i32>(0, out.mergeRadiusPx);
    return out;
}

} // namespace DeadPixelQC
