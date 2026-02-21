#include "ExportDefectMap.h"

#include <fstream>

namespace DeadPixelQC {

bool ExportDefectMap::write(const std::string& path, const DefectMap& map) {
    std::ofstream out(path, std::ios::out | std::ios::trunc);
    if (!out.is_open()) {
        return false;
    }

    out << "{\n";
    out << "  \"metadata\": {\n";
    out << "    \"width\": " << map.metadata().width << ",\n";
    out << "    \"height\": " << map.metadata().height << ",\n";
    out << "    \"framesAnalyzed\": " << map.metadata().framesAnalyzed << ",\n";
    out << "    \"sourceClip\": \"" << map.metadata().sourceClip << "\"\n";
    out << "  },\n";
    out << "  \"points\": [\n";

    const auto& points = map.points();
    for (std::size_t i = 0; i < points.size(); ++i) {
        const DefectPointNorm& p = points[i];
        out << "    {\"u\": " << p.u << ", \"v\": " << p.v << "}";
        if (i + 1 < points.size()) {
            out << ",";
        }
        out << "\n";
    }

    out << "  ]\n";
    out << "}\n";
    return out.good();
}

} // namespace DeadPixelQC
