#include "ExportJSON.h"

#include <fstream>

namespace DeadPixelQC {

bool ExportJSON::writeEvents(const std::string& path, const EventModel& model) {
    std::ofstream out(path, std::ios::out | std::ios::trunc);
    if (!out.is_open()) {
        return false;
    }

    out << "{\n";
    out << "  \"events\": [\n";

    bool firstEvent = true;
    for (std::size_t i = 0; i < model.size(); ++i) {
        const DetectionEvent& event = model.get(i);
        for (const DetectionHit& hit : event.hits) {
            if (!firstEvent) {
                out << ",\n";
            }
            firstEvent = false;
            out << "    {\"frame\":" << event.frameIndex
                << ",\"x\":" << hit.x
                << ",\"y\":" << hit.y
                << ",\"confidence\":" << hit.confidence << "}";
        }
    }

    out << "\n";
    out << "  ]\n";
    out << "}\n";

    return out.good();
}

} // namespace DeadPixelQC
