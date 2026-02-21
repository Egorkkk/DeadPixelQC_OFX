#include "ImportDefectMap.h"

#include <fstream>
#include <sstream>

namespace DeadPixelQC {

bool ImportDefectMap::read(const std::string& path, DefectMap& outMap) {
    std::ifstream in(path);
    if (!in.is_open()) {
        return false;
    }

    // Stub only: parser logic will be implemented in a follow-up.
    // Current behavior accepts readable files and leaves map empty.
    std::stringstream buffer;
    buffer << in.rdbuf();
    (void)buffer;

    outMap.clear();
    return true;
}

} // namespace DeadPixelQC
