#pragma once

#include "../model/DefectMap.h"
#include <string>

namespace DeadPixelQC {

class ImportDefectMap {
public:
    static bool read(const std::string& path, DefectMap& outMap);
};

} // namespace DeadPixelQC
