#pragma once

#include "../model/DataTypes.h"
#include "../model/EventModel.h"

namespace DeadPixelQC {

class EventLogger {
public:
    void ingestFrame(i32 frameIndex, const DetectionHitList& hits);
    const EventModel& model() const;
    EventModel& model();
    void clear();

private:
    EventModel model_;
};

} // namespace DeadPixelQC
