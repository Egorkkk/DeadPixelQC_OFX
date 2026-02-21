#pragma once

#include "../../core/SpatialDetector.h"

namespace DeadPixelQC_OFX::Workers {

enum class FramePixelFormat {
    Unknown = 0,
    RGB8,
    RGBA8,
    RGB16,
    RGBA16,
    RGB32F,
    RGBA32F,
};

struct FrameView {
    void* data = nullptr;
    int width = 0;
    int height = 0;
    int rowBytes = 0;
    FramePixelFormat format = FramePixelFormat::Unknown;
};

enum class SpatialOutputMode {
    PassThrough = 0,
    CandidatesOverlay = 1,
    MaskOnly = 2,
};

struct SpatialParams {
    bool enable = true;
    float lumaThreshold = 0.98f;
    float whitenessThreshold = 0.05f;
    float robustZ = 10.0f;
    int maxClusterArea = 4;
    SpatialOutputMode outputMode = SpatialOutputMode::PassThrough;
};

struct SpatialResult {
    struct DebugStats {
        struct SampleRGBA {
            int x = 0;
            int y = 0;
            float r = 0.0f;
            float g = 0.0f;
            float b = 0.0f;
            float a = 1.0f;
        };

        int numCandidates = 0;
        float maxScore = 0.0f;
        float minLuma = 0.0f;
        float maxLuma = 0.0f;
        SampleRGBA p0;
        SampleRGBA p1;
    };

    bool ok = true;
    bool processed = false;
    DeadPixelQC::FrameDetection detection;
    DebugStats debug;
};

struct SpatialWorker {
    SpatialResult process(const FrameView& in, FrameView& out, const SpatialParams& p);
};

} // namespace DeadPixelQC_OFX::Workers
