#include "TemporalWorker.h"

namespace DeadPixelQC_OFX::Workers {

TemporalResult TemporalWorker::process(const FrameView& /*in*/, FrameView& /*out*/, const TemporalParams& /*p*/) {
    return TemporalResult{};
}

} // namespace DeadPixelQC_OFX::Workers
