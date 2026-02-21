#include "ExportDefectMap.h"

#include <cmath>
#include <fstream>
#include <iomanip>

namespace DeadPixelQC {

namespace {

std::string escapeJsonString(const std::string& text) {
    std::string out;
    out.reserve(text.size() + 8);
    for (char ch : text) {
        switch (ch) {
            case '\"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default: out += ch; break;
        }
    }
    return out;
}

float finiteOrZero(float value) {
    return std::isfinite(value) ? value : 0.0f;
}

} // namespace

bool ExportDefectMap::write(const std::string& path, const DefectMap& map) {
    std::ofstream out(path, std::ios::out | std::ios::trunc);
    if (!out.is_open()) {
        return false;
    }

    out << std::setprecision(9);
    out << "{\n";
    out << "  \"metadata\": {\n";
    out << "    \"version\": " << map.metadata().version << ",\n";
    out << "    \"sourceWidth\": " << map.metadata().sourceWidth << ",\n";
    out << "    \"sourceHeight\": " << map.metadata().sourceHeight << ",\n";
    out << "    \"framesAnalyzed\": " << map.metadata().framesAnalyzed << ",\n";
    out << "    \"createdUtc\": \"" << escapeJsonString(map.metadata().createdUtc) << "\",\n";
    out << "    \"cameraTag\": \"" << escapeJsonString(map.metadata().cameraTag) << "\"\n";
    out << "  },\n";
    out << "  \"points\": [\n";

    const auto& points = map.points();
    for (std::size_t i = 0; i < points.size(); ++i) {
        const DefectPointNorm& p = points[i];
        out << "    {\"u\": " << finiteOrZero(p.u)
            << ", \"v\": " << finiteOrZero(p.v)
            << ", \"weight\": " << finiteOrZero(p.weight) << "}";
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
