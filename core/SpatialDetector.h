#pragma once

#include "Types.h"
#include "PixelFormatAdapter.h"
#include "ColorMetrics.h"
#include "ConnectedComponents.h"
#include <vector>
#include <memory>

namespace DeadPixelQC {

// Configuration for spatial detection (Phase 1)
struct SpatialDetectorParams {
    ColorGateParams colorGate;
    RobustContrastParams contrastGate;
    i32 maxClusterArea = 4;          // Amax (default 4)
    bool useFloodFill = false;       // Use flood fill instead of union-find
    
    bool validate() const {
        return colorGate.validate() && 
               contrastGate.validate() && 
               maxClusterArea > 0;
    }
};

// Main spatial detector class
class SpatialDetector {
public:
    SpatialDetector() = default;
    
    explicit SpatialDetector(const SpatialDetectorParams& params)
        : params_(params) {}
    
    // Set parameters
    void setParams(const SpatialDetectorParams& params) {
        params_ = params;
    }
    
    // Get current parameters
    const SpatialDetectorParams& getParams() const {
        return params_;
    }
    
    // Process a single frame
    FrameDetection processFrame(const ImageBuffer& image, i32 frameIndex = -1) {
        FrameDetection result;
        result.frameIndex = frameIndex;
        
        if (!image.isValid() || !params_.validate()) {
            return result;
        }
        
        const i32 width = image.width();
        const i32 height = image.height();
        const i32 pixelCount = width * height;
        
        // Create candidate mask
        std::vector<bool> candidateMask(pixelCount, false);
        
        // Detect candidate pixels
        detectCandidates(image, candidateMask);
        
        // Find connected components
        std::vector<Component> components;
        if (params_.useFloodFill) {
            components = ConnectedComponents::findComponentsFloodFill(
                candidateMask, width, height, params_.maxClusterArea);
        } else {
            components = ConnectedComponents::findComponents(
                candidateMask, width, height, params_.maxClusterArea);
        }
        
        result.candidates = std::move(components);
        return result;
    }
    
    // Process frame and return mask image (for MaskOnly mode)
    std::vector<u8> createMask(const ImageBuffer& image, i32 frameIndex = -1) {
        FrameDetection detection = processFrame(image, frameIndex);
        
        const i32 width = image.width();
        const i32 height = image.height();
        std::vector<u8> mask(width * height, 0);
        
        for (const auto& comp : detection.candidates) {
            for (const auto& pixel : comp.pixels) {
                i32 idx = pixel.y * width + pixel.x;
                mask[idx] = 255;
            }
        }
        
        return mask;
    }
    
private:
    // Detect candidate pixels and fill mask
    void detectCandidates(const ImageBuffer& image, std::vector<bool>& mask) {
        const i32 width = image.width();
        const i32 height = image.height();
        
        // Process interior pixels (skip borders where neighborhood sampling would be incomplete)
        const i32 border = params_.contrastGate.radius();
        
        for (i32 y = border; y < height - border; ++y) {
            for (i32 x = border; x < width - border; ++x) {
                i32 idx = y * width + x;
                
                if (ColorMetrics::isCandidatePixel(image, x, y, 
                                                   params_.colorGate, 
                                                   params_.contrastGate)) {
                    mask[idx] = true;
                }
            }
        }
    }
    
private:
    SpatialDetectorParams params_;
};

} // namespace DeadPixelQC