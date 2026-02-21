#include "SpatialWorker.h"

#include "../../core/PixelFormatAdapter.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>

namespace DeadPixelQC_OFX::Workers {

namespace {

DeadPixelQC::PixelFormat toCoreFormat(FramePixelFormat format) {
    switch (format) {
        case FramePixelFormat::RGB8: return DeadPixelQC::PixelFormat::RGB8;
        case FramePixelFormat::RGBA8: return DeadPixelQC::PixelFormat::RGBA8;
        case FramePixelFormat::RGB16: return DeadPixelQC::PixelFormat::RGB16;
        case FramePixelFormat::RGBA16: return DeadPixelQC::PixelFormat::RGBA16;
        case FramePixelFormat::RGB32F: return DeadPixelQC::PixelFormat::RGB32F;
        case FramePixelFormat::RGBA32F: return DeadPixelQC::PixelFormat::RGBA32F;
        default: return DeadPixelQC::PixelFormat::Unknown;
    }
}

void copyRows(const FrameView& in, FrameView& out) {
    if (!in.data || !out.data) {
        return;
    }
    if (in.width <= 0 || in.height <= 0 || out.width <= 0 || out.height <= 0) {
        return;
    }
    if (in.rowBytes == 0 || out.rowBytes == 0) {
        return;
    }

    const int width = std::min(in.width, out.width);
    const int height = std::min(in.height, out.height);
    if (width <= 0 || height <= 0) {
        return;
    }

    const auto srcFormat = toCoreFormat(in.format);
    const auto dstFormat = toCoreFormat(out.format);
    if (srcFormat == DeadPixelQC::PixelFormat::Unknown || srcFormat != dstFormat) {
        return;
    }

    const int bytesPerPixel = DeadPixelQC::getBytesPerPixel(srcFormat);
    if (bytesPerPixel <= 0) {
        return;
    }

    const int srcStride = in.rowBytes < 0 ? -in.rowBytes : in.rowBytes;
    const int dstStride = out.rowBytes < 0 ? -out.rowBytes : out.rowBytes;
    const int requestedBytes = width * bytesPerPixel;
    const int bytesToCopy = std::min(requestedBytes, std::min(srcStride, dstStride));
    if (bytesToCopy <= 0) {
        return;
    }

    const auto* srcBase = static_cast<const std::uint8_t*>(in.data);
    auto* dstBase = static_cast<std::uint8_t*>(out.data);

    if (in.rowBytes < 0) {
        srcBase += static_cast<std::ptrdiff_t>(srcStride) * (height - 1);
    }
    if (out.rowBytes < 0) {
        dstBase += static_cast<std::ptrdiff_t>(dstStride) * (height - 1);
    }

    for (int y = 0; y < height; ++y) {
        const std::uint8_t* srcRow = (in.rowBytes < 0) ? (srcBase - static_cast<std::ptrdiff_t>(srcStride) * y)
                                                        : (srcBase + static_cast<std::ptrdiff_t>(srcStride) * y);
        std::uint8_t* dstRow = (out.rowBytes < 0) ? (dstBase - static_cast<std::ptrdiff_t>(dstStride) * y)
                                                  : (dstBase + static_cast<std::ptrdiff_t>(dstStride) * y);
        std::memcpy(dstRow, srcRow, static_cast<size_t>(bytesToCopy));
    }
}

void renderMask(DeadPixelQC::ImageBuffer& output, const DeadPixelQC::FrameDetection& detection) {
    for (int y = 0; y < output.height(); ++y) {
        for (int x = 0; x < output.width(); ++x) {
            output.setPixelNormalized(x, y, 0.0f, 0.0f, 0.0f);
        }
    }
    for (const auto& component : detection.candidates) {
        for (const auto& pixel : component.pixels) {
            if (pixel.x >= 0 && pixel.x < output.width() && pixel.y >= 0 && pixel.y < output.height()) {
                output.setPixelNormalized(pixel.x, pixel.y, 1.0f, 1.0f, 1.0f);
            }
        }
    }
}

void renderOverlay(DeadPixelQC::ImageBuffer& output, const DeadPixelQC::FrameDetection& detection) {
    constexpr float alpha = 0.75f;
    for (const auto& component : detection.candidates) {
        for (const auto& pixel : component.pixels) {
            if (pixel.x < 0 || pixel.x >= output.width() || pixel.y < 0 || pixel.y >= output.height()) {
                continue;
            }
            float r = 0.0f;
            float g = 0.0f;
            float b = 0.0f;
            output.getRGBNormalized(pixel.x, pixel.y, r, g, b);
            const float outR = r * (1.0f - alpha) + 1.0f * alpha;
            const float outG = g * (1.0f - alpha) + 1.0f * alpha;
            const float outB = b * (1.0f - alpha);
            output.setPixelNormalized(pixel.x, pixel.y, outR, outG, outB);
        }
    }
}

void computeLumaRange(const DeadPixelQC::ImageBuffer& input, float& minLuma, float& maxLuma) {
    const int width = input.width();
    const int height = input.height();
    if (width <= 0 || height <= 0) {
        minLuma = 0.0f;
        maxLuma = 0.0f;
        return;
    }

    float minValue = std::numeric_limits<float>::max();
    float maxValue = std::numeric_limits<float>::lowest();
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            float r = 0.0f;
            float g = 0.0f;
            float b = 0.0f;
            input.getRGBNormalized(x, y, r, g, b);
            const float luma = DeadPixelQC::ColorUtils::computeLuma(r, g, b);
            minValue = std::min(minValue, luma);
            maxValue = std::max(maxValue, luma);
        }
    }

    minLuma = minValue;
    maxLuma = maxValue;
}

float computeCandidateMaxLumaScore(const DeadPixelQC::ImageBuffer& input,
                                   const DeadPixelQC::FrameDetection& detection) {
    float maxScore = 0.0f;
    for (const auto& component : detection.candidates) {
        for (const auto& pixel : component.pixels) {
            if (pixel.x < 0 || pixel.x >= input.width() || pixel.y < 0 || pixel.y >= input.height()) {
                continue;
            }
            float r = 0.0f;
            float g = 0.0f;
            float b = 0.0f;
            input.getRGBNormalized(pixel.x, pixel.y, r, g, b);
            const float luma = DeadPixelQC::ColorUtils::computeLuma(r, g, b);
            maxScore = std::max(maxScore, luma);
        }
    }
    return maxScore;
}

int clampCoord(int value, int maxInclusive) {
    if (maxInclusive < 0) {
        return 0;
    }
    return std::max(0, std::min(value, maxInclusive));
}

void sampleDebugPixels(const DeadPixelQC::ImageBuffer& input, SpatialResult::DebugStats& debug) {
    const int width = input.width();
    const int height = input.height();
    if (width <= 0 || height <= 0) {
        return;
    }

    debug.p0.x = clampCoord(width / 2, width - 1);
    debug.p0.y = clampCoord(height / 2, height - 1);
    debug.p1.x = clampCoord(10, width - 1);
    debug.p1.y = clampCoord(10, height - 1);

    input.getPixelNormalized(debug.p0.x, debug.p0.y, debug.p0.r, debug.p0.g, debug.p0.b, debug.p0.a);
    input.getPixelNormalized(debug.p1.x, debug.p1.y, debug.p1.r, debug.p1.g, debug.p1.b, debug.p1.a);
}

} // namespace

SpatialResult SpatialWorker::process(const FrameView& in, FrameView& out, const SpatialParams& p) {
    SpatialResult result;

    copyRows(in, out);

    if (!p.enable) {
        return result;
    }

    const DeadPixelQC::PixelFormat inputFormat = toCoreFormat(in.format);
    const DeadPixelQC::PixelFormat outputFormat = toCoreFormat(out.format);
    if (inputFormat == DeadPixelQC::PixelFormat::Unknown || outputFormat == DeadPixelQC::PixelFormat::Unknown) {
        result.ok = false;
        return result;
    }

    DeadPixelQC::ImageBuffer input(in.data, inputFormat, in.width, in.height, in.rowBytes);
    DeadPixelQC::ImageBuffer output(out.data, outputFormat, out.width, out.height, out.rowBytes);
    if (!input.isValid() || !output.isValid()) {
        result.ok = false;
        return result;
    }

    DeadPixelQC::SpatialDetectorParams detectorParams;
    detectorParams.colorGate.lumaThreshold = p.lumaThreshold;
    detectorParams.colorGate.whitenessThreshold = p.whitenessThreshold;
    detectorParams.colorGate.useSaturation = false;
    detectorParams.contrastGate.neighborhood = DeadPixelQC::RobustContrastParams::Neighborhood::ThreeByThree;
    detectorParams.contrastGate.zScoreThreshold = p.robustZ;
    detectorParams.minClusterArea = 1;
    detectorParams.maxClusterArea = std::max(1, p.maxClusterArea);
    detectorParams.useFloodFill = false;

    DeadPixelQC::SpatialDetector detector(detectorParams);
    result.detection = detector.processFrame(input, -1);
    result.processed = true;

    result.debug.numCandidates = static_cast<int>(result.detection.candidates.size());
    computeLumaRange(input, result.debug.minLuma, result.debug.maxLuma);
    // Detector does not currently expose per-hit confidence; approximate score by candidate max luma.
    result.debug.maxScore = computeCandidateMaxLumaScore(input, result.detection);
    sampleDebugPixels(input, result.debug);

    if (p.outputMode == SpatialOutputMode::MaskOnly) {
        renderMask(output, result.detection);
    } else if (p.outputMode == SpatialOutputMode::CandidatesOverlay) {
        renderOverlay(output, result.detection);
    }

    return result;
}

} // namespace DeadPixelQC_OFX::Workers
