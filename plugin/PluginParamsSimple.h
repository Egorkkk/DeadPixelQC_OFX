#pragma once

#include <cstdint>

namespace DeadPixelQC {

// Simple parameter structure for demo
struct PluginParams {
    bool enable = true;
    int viewMode = 0; // 0: Output, 1: CandidatesOverlay, 2: ConfirmedOverlay, 3: MaskOnly
    float lumaThreshold = 0.98f;
    float whitenessThreshold = 0.05f;
    int neighborhood = 0; // 0: 3x3, 1: 5x5
    float robustZ = 10.0f;
    int maxClusterArea = 4;
    int temporalMode = 1; // 0: Off, 1: SequentialOnly
    int stuckWindowFrames = 60;
    float stuckMinFraction = 0.95f;
    int maxGapFrames = 2;
    bool repairEnable = false;
    int repairMethod = 0; // 0: NeighborMedian, 1: DirectionalMedian
};

} // namespace DeadPixelQC