#pragma once

#include "../model/DataTypes.h"
#include "../model/EventModel.h"

#include <cstdint>
#include <unordered_map>

namespace DeadPixelQC {

class EventLogger {
public:
    struct Params {
        i32 minPersistenceFrames = 2;
        i32 maxGapFrames = 1;
        i32 cooldownFrames = 10;
        i32 mergeRadiusPx = 0;
    };

    EventLogger() = default;
    explicit EventLogger(const Params& params);

    void setParams(const Params& params);
    const Params& params() const;

    void ingestFrame(i32 frameIndex, const DetectionHitList& hits);
    const EventModel& model() const;
    EventModel& model();
    void clear();

private:
    struct TrackState {
        i32 lastSeenFrame = -1;
        i32 persistenceCount = 0;
        i32 cooldownUntilFrame = -1;
    };

    static std::uint64_t makeKey(i32 x, i32 y);
    std::uint64_t resolveTrackKey(i32 x, i32 y) const;
    static Params sanitizeParams(const Params& params);

private:
    Params params_;
    std::unordered_map<std::uint64_t, TrackState> tracks_;
    EventModel model_;
};

} // namespace DeadPixelQC
