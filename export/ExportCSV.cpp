#include "ExportCSV.h"

#include <fstream>

namespace DeadPixelQC {

bool ExportCSV::writeEvents(const std::string& path, const EventModel& model) {
    std::ofstream out(path, std::ios::out | std::ios::trunc);
    if (!out.is_open()) {
        return false;
    }

    out << "frame,x,y,confidence\n";

    for (std::size_t i = 0; i < model.size(); ++i) {
        const DetectionEvent& event = model.get(i);
        for (const DetectionHit& hit : event.hits) {
            out << event.frameIndex << "," << hit.x << "," << hit.y << "," << hit.confidence << "\n";
        }
    }

    return out.good();
}

} // namespace DeadPixelQC
