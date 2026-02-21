#pragma once

#include "DataTypes.h"
#include <cstddef>
#include <vector>

namespace DeadPixelQC {

class EventModel {
public:
    void clear();
    std::size_t size() const;
    const DetectionEvent& get(std::size_t i) const;
    void addEvent(const DetectionEvent& event);

private:
    std::vector<DetectionEvent> events_;
};

} // namespace DeadPixelQC
