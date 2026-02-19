#include "SyntheticGenerator.h"
#include <algorithm>
#include <random>
#include <cmath>

namespace DeadPixelQC {

// Generate synthetic test image with stuck pixels
std::vector<u8> SyntheticGenerator::generateTestImage(i32 width, i32 height, 
                                                       const TestConfig& config) {
    std::vector<u8> image(width * height * 3, 0); // RGB format
    
    // Fill with background
    for (i32 y = 0; y < height; ++y) {
        for (i32 x = 0; x < width; ++x) {
            i32 idx = (y * width + x) * 3;
            
            // Add some noise to background
            f32 noise = config.backgroundNoise * (rand() / (f32)RAND_MAX - 0.5f);
            f32 value = config.backgroundBrightness + noise;
            value = std::clamp(value, 0.0f, 1.0f);
            
            u8 pixelValue = static_cast<u8>(value * 255);
            image[idx] = pixelValue;     // R
            image[idx + 1] = pixelValue; // G
            image[idx + 2] = pixelValue; // B
        }
    }
    
    // Add stuck pixels
    for (const auto& stuck : config.stuckPixels) {
        if (stuck.x >= 0 && stuck.x < width && stuck.y >= 0 && stuck.y < height) {
            i32 idx = (stuck.y * width + stuck.x) * 3;
            
            u8 pixelValue = static_cast<u8>(stuck.brightness * 255);
            image[idx] = pixelValue;     // R
            image[idx + 1] = pixelValue; // G
            image[idx + 2] = pixelValue; // B
        }
    }
    
    // Add moving highlights (false positive tests)
    for (const auto& highlight : config.movingHighlights) {
        if (highlight.x >= 0 && highlight.x < width && highlight.y >= 0 && highlight.y < height) {
            i32 idx = (highlight.y * width + highlight.x) * 3;
            
            u8 pixelValue = static_cast<u8>(highlight.brightness * 255);
            image[idx] = pixelValue;     // R
            image[idx + 1] = pixelValue; // G
            image[idx + 2] = pixelValue; // B
        }
    }
    
    // Add subtitle-like edges
    for (const auto& subtitle : config.subtitles) {
        i32 startY = subtitle.y;
        i32 endY = std::min(startY + subtitle.height, height);
        
        for (i32 y = startY; y < endY; ++y) {
            for (i32 x = subtitle.x; x < subtitle.x + subtitle.width && x < width; ++x) {
                i32 idx = (y * width + x) * 3;
                
                u8 pixelValue = static_cast<u8>(subtitle.brightness * 255);
                image[idx] = pixelValue;     // R
                image[idx + 1] = pixelValue; // G
                image[idx + 2] = pixelValue; // B
            }
        }
    }
    
    return image;
}

// Generate sequence of test images
std::vector<std::vector<u8>> SyntheticGenerator::generateTestSequence(
    i32 width, i32 height, i32 numFrames, const SequenceConfig& config) {
    
    std::vector<std::vector<u8>> sequence;
    sequence.reserve(numFrames);
    
    // Generate stuck pixels at fixed positions
    std::vector<StuckPixel> stuckPixels;
    for (const auto& pos : config.stuckPositions) {
        stuckPixels.push_back({pos.x, pos.y, config.stuckBrightness});
    }
    
    // Generate moving highlights
    std::vector<MovingHighlightTrajectory> movingHighlights;
    for (i32 i = 0; i < config.numMovingHighlights; ++i) {
        i32 startX = rand() % width;
        i32 startY = rand() % height;
        i32 speedX = (rand() % 5) - 2; // -2 to 2
        i32 speedY = (rand() % 5) - 2;
        
        movingHighlights.push_back({startX, startY, speedX, speedY, config.highlightBrightness});
    }
    
    // Generate frames
    for (i32 frame = 0; frame < numFrames; ++frame) {
        TestConfig frameConfig;
        frameConfig.backgroundBrightness = config.backgroundBrightness;
        frameConfig.backgroundNoise = config.backgroundNoise;
        
        // Add stuck pixels (same positions every frame)
        frameConfig.stuckPixels = stuckPixels;
        
        // Add moving highlights (update positions)
        for (auto& highlight : movingHighlights) {
            i32 x = highlight.startX + highlight.speedX * frame;
            i32 y = highlight.startY + highlight.speedY * frame;
            
            // Wrap around
            x = (x % width + width) % width;
            y = (y % height + height) % height;
            
            frameConfig.movingHighlights.push_back({x, y, highlight.brightness});
        }
        
        // Add subtitles on some frames
        if (frame >= config.subtitleStartFrame && frame < config.subtitleEndFrame) {
            for (const auto& subtitle : config.subtitles) {
                frameConfig.subtitles.push_back(subtitle);
            }
        }
        
        // Generate frame
        sequence.push_back(generateTestImage(width, height, frameConfig));
        
        // Clear moving highlights for next frame
        frameConfig.movingHighlights.clear();
        frameConfig.subtitles.clear();
    }
    
    return sequence;
}

// Create ImageBuffer from raw data
ImageBuffer SyntheticGenerator::createImageBuffer(const std::vector<u8>& rgbData, 
                                                  i32 width, i32 height) {
    if (rgbData.size() != static_cast<size_t>(width * height * 3)) {
        return ImageBuffer();
    }
    
    // Note: We need to cast away const for ImageBuffer constructor
    void* data = const_cast<void*>(static_cast<const void*>(rgbData.data()));
    return ImageBuffer(data, PixelFormat::RGB8, width, height);
}

// Generate ground truth mask for stuck pixels
std::vector<bool> SyntheticGenerator::generateGroundTruthMask(i32 width, i32 height,
                                                              const std::vector<StuckPixel>& stuckPixels) {
    std::vector<bool> mask(width * height, false);
    
    for (const auto& stuck : stuckPixels) {
        if (stuck.x >= 0 && stuck.x < width && stuck.y >= 0 && stuck.y < height) {
            i32 idx = stuck.y * width + stuck.x;
            mask[idx] = true;
        }
    }
    
    return mask;
}

// Validate detection results against ground truth
ValidationResult SyntheticGenerator::validateDetection(
    const std::vector<Component>& detected,
    const std::vector<bool>& groundTruth,
    i32 width, i32 height) {
    
    ValidationResult result;
    
    // Convert ground truth to component list for comparison
    std::vector<bool> visited(width * height, false);
    
    // Mark detected pixels
    for (const auto& comp : detected) {
        for (const auto& pixel : comp.pixels) {
            i32 idx = pixel.y * width + pixel.x;
            if (idx >= 0 && idx < width * height) {
                visited[idx] = true;
            }
        }
    }
    
    // Count true positives, false positives, false negatives
    i32 truePositives = 0;
    i32 falsePositives = 0;
    i32 falseNegatives = 0;
    
    for (i32 i = 0; i < width * height; ++i) {
        bool isStuck = groundTruth[i];
        bool isDetected = visited[i];
        
        if (isStuck && isDetected) {
            truePositives++;
        } else if (isStuck && !isDetected) {
            falseNegatives++;
        } else if (!isStuck && isDetected) {
            falsePositives++;
        }
    }
    
    result.truePositives = truePositives;
    result.falsePositives = falsePositives;
    result.falseNegatives = falseNegatives;
    
    // Compute metrics
    i32 totalStuck = truePositives + falseNegatives;
    i32 totalDetected = truePositives + falsePositives;
    
    if (totalStuck > 0) {
        result.recall = static_cast<f32>(truePositives) / totalStuck;
    }
    
    if (totalDetected > 0) {
        result.precision = static_cast<f32>(truePositives) / totalDetected;
    }
    
    if (result.precision > 0 && result.recall > 0) {
        result.f1Score = 2.0f * result.precision * result.recall / 
                        (result.precision + result.recall);
    }
    
    return result;
}

} // namespace DeadPixelQC