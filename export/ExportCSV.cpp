#include "ExportCSV.h"

#include <filesystem>
#include <fstream>
#include <system_error>

namespace DeadPixelQC {

bool ExportCSV::writeEvents(const std::string& path, const EventModel& model) {
    if (path.empty()) {
        return false;
    }

    try {
        const std::filesystem::path fsPath(path);
        const std::filesystem::path parent = fsPath.parent_path();
        if (!parent.empty()) {
            std::error_code ec;
            std::filesystem::create_directories(parent, ec);
            if (ec) {
                return false;
            }
        }

        std::ofstream out(path, std::ios::out | std::ios::trunc);
        if (!out.is_open()) {
            return false;
        }

        out << "frame,x,y,confidence,persistence,confirmed\n";

        for (std::size_t i = 0; i < model.size(); ++i) {
            const DetectionEvent& event = model.get(i);
            for (const DetectionHit& hit : event.hits) {
                out << event.frameIndex << ","
                    << hit.x << "," << hit.y << "," << hit.confidence << ","
                    << event.persistence << ","
                    << (event.confirmed ? 1 : 0) << "\n";
            }
        }

        return out.good();
    } catch (...) {
        return false;
    }
}

} // namespace DeadPixelQC
