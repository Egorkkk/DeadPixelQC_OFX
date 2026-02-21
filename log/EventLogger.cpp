#include "EventLogger.h"

namespace DeadPixelQC {

void EventLogger::ingestFrame(i32 frameIndex, const DetectionHitList& hits) {
    // Stub only: event logging logic will be implemented in a follow-up.
    (void)frameIndex;
    (void)hits;
}

const EventModel& EventLogger::model() const {
    return model_;
}

EventModel& EventLogger::model() {
    return model_;
}

void EventLogger::clear() {
    model_.clear();
}

} // namespace DeadPixelQC
