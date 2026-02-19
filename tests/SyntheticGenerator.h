#pragma once

#include "../core/Types.h"
#include "../core/PixelFormatAdapter.h"
#include "../core/ConnectedComponents.h"
#include <vector>
#include <cstdlib>

namespace DeadPixelQC {

// Test configuration structures
struct StuckPixel {
    i32 x = 0;
    i32 y = 0;
    f32 brightness = 1.0f; // Normalized [0..1]
};

struct MovingHighlight {
    i32 x = 0;
    i32 y = 0;
    f32 brightness = 1.0f;
};

struct Subtitle {
    i32 x = 0;
    i32 y = 0;
    i32 width = 100;
    i32 height = 30;
    f32 brightness = 1.0f;
};

struct TestConfig {
    f32 backgroundBrightness = 0.1f;
    f32 backgroundNoise = 0.05f;
    std::vector<StuckPixel> stuckPixels;
    std::vector<MovingHighlight> movingHighlights;
    std::vector<Subtitle> subtitles;
};

struct MovingHighlightTrajectory {
    i32 startX = 0;
    i32 startY = 0;
    i32 speedX = 0;
    i32 speedY = 0;
    f32 brightness = 1.0f;
};

struct SequenceConfig {
    f32 backgroundBrightness = 0.1f;
    f32 backgroundNoise = 0.05f;
    f32 stuckBrightness = 0.98f;
    f32 highlightBrightness = 0.95f;
    
    std::vector<PixelCoord> stuckPositions;
    i32 numMovingHighlights = 3;
    
    std::vector<Subtitle> subtitles;
    i32 subtitleStartFrame = 10;
    i32 subtitleEndFrame = 20;
};

struct ValidationResult {
    i32 truePositives = 0;
    i32 falsePositives = 0;
    i32 falseNegatives = 0;
    f32 precision = 0.0f;
    f32 recall = 0.0f;
    f32 f1Score = 0.0f;
};

// Synthetic test image generator
class SyntheticGenerator {
public:
    // Generate a single test image
    static std::vector<u8> generateTestImage(i32 width, i32 height, 
                                             const TestConfig& config);
    
    // Generate a sequence of test images
    static std::vector<std::vector<u8>> generateTestSequence(
        i32 width, i32 height, i32 numFrames, const SequenceConfig& config);
    
    // Create ImageBuffer from raw RGB data
    static ImageBuffer createImageBuffer(const std::vector<u8>& rgbData,
                                         i32 width, i32 height);
    
    // Generate ground truth mask for stuck pixels
    static std::vector<bool> generateGroundTruthMask(i32 width, i32 height,
                                                     const std::vector<StuckPixel>& stuckPixels);
    
    // Validate detection results against ground truth
    static ValidationResult validateDetection(
        const std::vector<Component>& detected,
        const std::vector<bool>& groundTruth,
        i32 width, i32 height);
    
    // Helper: create simple test with single stuck pixel
    static TestConfig createSingleStuckTest(i32 x, i32 y, f32 brightness = 0.98f) {
        TestConfig config;
        config.stuckPixels.push_back({x, y, brightness});
        return config;
    }
    
    // Helper: create test with cluster of stuck pixels
    static TestConfig createClusterTest(i32 centerX, i32 centerY, i32 radius = 1) {
        TestConfig config;
        for (i32 dy = -radius; dy <= radius; ++dy) {
            for (i32 dx = -radius; dx <= radius; ++dx) {
                config.stuckPixels.push_back({centerX + dx, centerY + dy, 0.98f});
            }
        }
        return config;
    }
    
    // Helper: create false positive test with moving highlight
    static TestConfig createMovingHighlightTest(i32 x, i32 y, f32 brightness = 0.95f) {
        TestConfig config;
        config.movingHighlights.push_back({x, y, brightness});
        return config;
    }
    
    // Helper: create false positive test with subtitle
    static TestConfig createSubtitleTest(i32 x, i32 y, i32 width = 100, i32 height = 30) {
        TestConfig config;
        config.subtitles.push_back({x, y, width, height, 1.0f});
        return config;
    }
};

} // namespace DeadPixelQC