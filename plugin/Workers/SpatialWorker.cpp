#include "SpatialWorker.h"

#include "../../core/PixelFormatAdapter.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>

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
    detectorParams.maxClusterArea = std::max(1, p.maxClusterArea);
    detectorParams.useFloodFill = false;

    DeadPixelQC::SpatialDetector detector(detectorParams);
    result.detection = detector.processFrame(input, -1);
    result.processed = true;

    if (p.outputMode == SpatialOutputMode::MaskOnly) {
        renderMask(output, result.detection);
    } else if (p.outputMode == SpatialOutputMode::CandidatesOverlay) {
        renderOverlay(output, result.detection);
    }

    return result;
}

} // namespace DeadPixelQC_OFX::Workers
