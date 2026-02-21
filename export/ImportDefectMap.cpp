#include "ImportDefectMap.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <utility>

namespace DeadPixelQC {

namespace {

void skipWhitespace(const std::string& text, std::size_t& pos) {
    while (pos < text.size() && std::isspace(static_cast<unsigned char>(text[pos])) != 0) {
        ++pos;
    }
}

bool findKey(const std::string& text, const char* key, std::size_t& keyPos) {
    const std::string needle = std::string("\"") + key + "\"";
    keyPos = text.find(needle);
    return keyPos != std::string::npos;
}

bool parseNumberAt(const std::string& text, std::size_t keyPos, double& outValue) {
    std::size_t pos = text.find(':', keyPos);
    if (pos == std::string::npos) {
        return false;
    }
    ++pos;
    skipWhitespace(text, pos);
    if (pos >= text.size()) {
        return false;
    }

    char* endPtr = nullptr;
    const double parsed = std::strtod(text.c_str() + pos, &endPtr);
    if (endPtr == text.c_str() + pos) {
        return false;
    }

    outValue = parsed;
    return true;
}

bool parseStringAt(const std::string& text, std::size_t keyPos, std::string& outValue) {
    std::size_t pos = text.find(':', keyPos);
    if (pos == std::string::npos) {
        return false;
    }
    ++pos;
    skipWhitespace(text, pos);
    if (pos >= text.size() || text[pos] != '"') {
        return false;
    }
    ++pos;

    std::string parsed;
    parsed.reserve(32);
    while (pos < text.size()) {
        const char ch = text[pos++];
        if (ch == '"') {
            outValue = parsed;
            return true;
        }
        if (ch == '\\' && pos < text.size()) {
            const char esc = text[pos++];
            switch (esc) {
                case '"': parsed.push_back('"'); break;
                case '\\': parsed.push_back('\\'); break;
                case '/': parsed.push_back('/'); break;
                case 'b': parsed.push_back('\b'); break;
                case 'f': parsed.push_back('\f'); break;
                case 'n': parsed.push_back('\n'); break;
                case 'r': parsed.push_back('\r'); break;
                case 't': parsed.push_back('\t'); break;
                default: parsed.push_back(esc); break;
            }
            continue;
        }
        parsed.push_back(ch);
    }

    return false;
}

bool parseNumberField(const std::string& text, const char* key, double& outValue) {
    std::size_t keyPos = std::string::npos;
    if (!findKey(text, key, keyPos)) {
        return false;
    }
    return parseNumberAt(text, keyPos, outValue);
}

bool parseStringField(const std::string& text, const char* key, std::string& outValue) {
    std::size_t keyPos = std::string::npos;
    if (!findKey(text, key, keyPos)) {
        return false;
    }
    return parseStringAt(text, keyPos, outValue);
}

float clampNorm(double value) {
    if (value < 0.0) {
        return 0.0f;
    }
    if (value > 1.0) {
        return 1.0f;
    }
    return static_cast<float>(value);
}

bool parsePointsArray(const std::string& jsonText, DefectMap& outMap) {
    std::size_t pointsKey = std::string::npos;
    if (!findKey(jsonText, "points", pointsKey)) {
        return false;
    }

    const std::size_t arrayStart = jsonText.find('[', pointsKey);
    if (arrayStart == std::string::npos) {
        return false;
    }
    const std::size_t arrayEnd = jsonText.find(']', arrayStart + 1);
    if (arrayEnd == std::string::npos) {
        return false;
    }

    std::size_t pos = arrayStart + 1;
    while (pos < arrayEnd) {
        const std::size_t objectStart = jsonText.find('{', pos);
        if (objectStart == std::string::npos || objectStart >= arrayEnd) {
            break;
        }
        const std::size_t objectEnd = jsonText.find('}', objectStart + 1);
        if (objectEnd == std::string::npos || objectEnd > arrayEnd) {
            return false;
        }

        const std::string objectText = jsonText.substr(objectStart, objectEnd - objectStart + 1);
        double u = 0.0;
        double v = 0.0;
        double weight = 1.0;
        if (!parseNumberField(objectText, "u", u) || !parseNumberField(objectText, "v", v)) {
            return false;
        }
        (void)parseNumberField(objectText, "weight", weight);

        DefectPointNorm p;
        p.u = clampNorm(u);
        p.v = clampNorm(v);
        p.weight = static_cast<float>(std::max(0.0, weight));
        outMap.addPoint(p);

        pos = objectEnd + 1;
    }

    return true;
}

} // namespace

bool ImportDefectMap::read(const std::string& path, DefectMap& outMap) {
    std::ifstream in(path);
    if (!in.is_open()) {
        return false;
    }

    std::stringstream buffer;
    buffer << in.rdbuf();
    const std::string jsonText = buffer.str();

    DefectMap parsed;
    double version = 1.0;
    double sourceWidth = 0.0;
    double sourceHeight = 0.0;
    double framesAnalyzed = 0.0;
    std::string createdUtc;
    std::string cameraTag;

    if (!parseNumberField(jsonText, "version", version) ||
        !parseNumberField(jsonText, "sourceWidth", sourceWidth) ||
        !parseNumberField(jsonText, "sourceHeight", sourceHeight)) {
        return false;
    }

    (void)parseNumberField(jsonText, "framesAnalyzed", framesAnalyzed);
    (void)parseStringField(jsonText, "createdUtc", createdUtc);
    (void)parseStringField(jsonText, "cameraTag", cameraTag);

    parsed.metadata().version = static_cast<i32>(version);
    parsed.metadata().sourceWidth = static_cast<i32>(sourceWidth);
    parsed.metadata().sourceHeight = static_cast<i32>(sourceHeight);
    parsed.metadata().framesAnalyzed = static_cast<i32>(framesAnalyzed);
    parsed.metadata().createdUtc = createdUtc;
    parsed.metadata().cameraTag = cameraTag;

    if (!parsePointsArray(jsonText, parsed)) {
        return false;
    }

    outMap = std::move(parsed);
    return true;
}

} // namespace DeadPixelQC
