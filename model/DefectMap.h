#pragma once

#include "DataTypes.h"
#include <vector>

namespace DeadPixelQC {

class DefectMap {
public:
    void clear();
    std::size_t size() const;
    void addPoint(const DefectPointNorm& p);

    const std::vector<DefectPointNorm>& points() const;
    std::vector<DefectPointNorm>& points();

    const DefectMapMetadata& metadata() const;
    DefectMapMetadata& metadata();

private:
    std::vector<DefectPointNorm> points_;
    DefectMapMetadata metadata_;
};

} // namespace DeadPixelQC
