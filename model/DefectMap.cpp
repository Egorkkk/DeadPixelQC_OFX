#include "DefectMap.h"

namespace DeadPixelQC {

void DefectMap::clear() {
    points_.clear();
    metadata_ = DefectMapMetadata{};
}

std::size_t DefectMap::size() const {
    return points_.size();
}

void DefectMap::addPoint(const DefectPointNorm& p) {
    points_.push_back(p);
}

const std::vector<DefectPointNorm>& DefectMap::points() const {
    return points_;
}

std::vector<DefectPointNorm>& DefectMap::points() {
    return points_;
}

const DefectMapMetadata& DefectMap::metadata() const {
    return metadata_;
}

DefectMapMetadata& DefectMap::metadata() {
    return metadata_;
}

} // namespace DeadPixelQC
