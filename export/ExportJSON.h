#pragma once

#include "../model/EventModel.h"
#include <string>

namespace DeadPixelQC {

class ExportJSON {
public:
    static bool writeEvents(const std::string& path, const EventModel& model);
};

} // namespace DeadPixelQC
