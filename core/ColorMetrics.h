#pragma once

#include "Types.h"
#include "PixelFormatAdapter.h"
#include <array>
#include <algorithm>
#include <cmath>

namespace DeadPixelQC {

// Configuration for color-based detection gates
struct ColorGateParams {
    f32 lumaThreshold = 0.98f;      // Y > Y_hi (default 0.98)
    f32 whitenessThreshold = 0.05f; // whiteness_metric < W_max (default 0.05)
    bool useSaturation = false;     // Use saturation instead of whiteness metric
    f32 saturationThreshold = 0.05f; // If using saturation
    
    // Validate parameters
    bool validate() const {
        return lumaThreshold >= 0.0f && lumaThreshold <= 1.0f &&
               whitenessThreshold >= 0.0f && whitenessThreshold <= 3.0f && // max possible whiteness is 3.0
               saturationThreshold >= 0.0f && saturationThreshold <= 1.0f;
    }
};

// Robust statistics for local contrast gate
struct RobustContrastParams {
    enum class Neighborhood {
        ThreeByThree,   // 3x3 neighborhood (8 samples)
        FiveByFive      // 5x5 neighborhood (24 samples)
    };
    
    Neighborhood neighborhood = Neighborhood::ThreeByThree;
    f32 zScoreThreshold = 10.0f;    // z >= z_min (default 10 for 3x3, 8 for 5x5)
    f32 epsilon = 1e-6f;            // Avoid division by zero
    
    // Get sample count for neighborhood
    i32 sampleCount() const {
        return (neighborhood == Neighborhood::ThreeByThree) ? 8 : 24;
    }
    
    // Get radius for neighborhood
    i32 radius() const {
        return (neighborhood == Neighborhood::ThreeByThree) ? 1 : 2;
    }
    
    bool validate() const {
        return zScoreThreshold > 0.0f && epsilon > 0.0f;
    }
};

// Color metrics calculator
class ColorMetrics {
public:
    // Check if pixel passes color gates
    static bool passesColorGate(f32 r, f32 g, f32 b, const ColorGateParams& params) {
        // Compute luma
        f32 luma = ColorUtils::computeLuma(r, g, b);
        
        // Luma gate
        if (luma <= params.lumaThreshold) {
            return false;
        }
        
        // Whiteness/saturation gate
        if (params.useSaturation) {
            f32 saturation = ColorUtils::computeSaturationFast(r, g, b);
            return saturation <= params.saturationThreshold;
        } else {
            f32 whiteness = ColorUtils::computeWhitenessFast(r, g, b);
            return whiteness <= params.whitenessThreshold;
        }
    }
    
    // Compute robust z-score for local contrast
    static f32 computeRobustZScore(const ImageBuffer& image, i32 x, i32 y, 
                                   const RobustContrastParams& params) {
        if (!image.isValid()) {
            return 0.0f;
        }
        
        const i32 radius = params.radius();
        const i32 width = image.width();
        const i32 height = image.height();
        
        // Get center pixel luma
        f32 r, g, b;
        image.getRGBNormalized(x, y, r, g, b);
        f32 centerLuma = ColorUtils::computeLuma(r, g, b);
        
        // Collect neighborhood samples (excluding center)
        std::array<f32, 24> samples; // Max for 5x5
        i32 sampleCount = 0;
        
        for (i32 dy = -radius; dy <= radius; ++dy) {
            for (i32 dx = -radius; dx <= radius; ++dx) {
                // Skip center pixel
                if (dx == 0 && dy == 0) {
                    continue;
                }
                
                i32 nx = x + dx;
                i32 ny = y + dy;
                
                // Check bounds
                if (nx >= 0 && nx < width && ny >= 0 && ny < height) {
                    image.getRGBNormalized(nx, ny, r, g, b);
                    samples[sampleCount++] = ColorUtils::computeLuma(r, g, b);
                }
            }
        }
        
        // Need at least 3 samples for meaningful statistics
        if (sampleCount < 3) {
            return 0.0f;
        }
        
        // Compute median
        std::sort(samples.begin(), samples.begin() + sampleCount);
        f32 median = samples[sampleCount / 2];
        
        // Compute MAD (Median Absolute Deviation)
        std::array<f32, 24> deviations;
        for (i32 i = 0; i < sampleCount; ++i) {
            deviations[i] = std::abs(samples[i] - median);
        }
        
        std::sort(deviations.begin(), deviations.begin() + sampleCount);
        f32 mad = deviations[sampleCount / 2];
        
        // Convert MAD to sigma estimate
        f32 sigma = MAD_SCALE * mad;
        
        // Compute robust z-score
        f32 z = (centerLuma - median) / (sigma + params.epsilon);
        return z;
    }
    
    // Check if pixel passes robust contrast gate
    static bool passesContrastGate(const ImageBuffer& image, i32 x, i32 y,
                                   const RobustContrastParams& params) {
        f32 z = computeRobustZScore(image, x, y, params);
        return z >= params.zScoreThreshold;
    }
    
    // Combined check: color gate + contrast gate
    static bool isCandidatePixel(const ImageBuffer& image, i32 x, i32 y,
                                 const ColorGateParams& colorParams,
                                 const RobustContrastParams& contrastParams) {
        // Get pixel color
        f32 r, g, b;
        image.getRGBNormalized(x, y, r, g, b);
        
        // Check color gate
        if (!passesColorGate(r, g, b, colorParams)) {
            return false;
        }
        
        // Check contrast gate
        return passesContrastGate(image, x, y, contrastParams);
    }
};

} // namespace DeadPixelQC