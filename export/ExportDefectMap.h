#pragma once

#include "../model/DefectMap.h"
#include <string>

namespace DeadPixelQC {

class ExportDefectMap {
public:
    static bool write(const std::string& path, const DefectMap& map);
};

} // namespace DeadPixelQC
