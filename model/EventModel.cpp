#include "EventModel.h"

#include <stdexcept>

namespace DeadPixelQC {

void EventModel::clear() {
    events_.clear();
}

std::size_t EventModel::size() const {
    return events_.size();
}

const DetectionEvent& EventModel::get(std::size_t i) const {
    if (i >= events_.size()) {
        throw std::out_of_range("EventModel::get index out of range");
    }
    return events_[i];
}

void EventModel::addEvent(const DetectionEvent& event) {
    events_.push_back(event);
}

} // namespace DeadPixelQC
