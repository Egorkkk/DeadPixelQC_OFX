#pragma once

#include "Types.h"
#include "PixelFormatAdapter.h"
#include <vector>
#include <array>
#include <algorithm>

namespace DeadPixelQC {

// Repair methods
enum class RepairMethod {
    NeighborMedian,     // Replace with median of neighbors (ring)
    DirectionalMedian   // Directional median to preserve edges
};

// Repair configuration
struct RepairParams {
    bool enable = false;
    RepairMethod method = RepairMethod::NeighborMedian;
    i32 kernelSize = 3; // 3x3 or 5x5
    
    bool validate() const {
        return kernelSize == 3 || kernelSize == 5;
    }
    
    i32 radius() const {
        return kernelSize / 2;
    }
};

// Pixel repair/fixer class
class Fixer {
public:
    Fixer() = default;
    
    explicit Fixer(const RepairParams& params)
        : params_(params) {}
    
    // Set parameters
    void setParams(const RepairParams& params) {
        params_ = params;
    }
    
    // Get current parameters
    const RepairParams& getParams() const {
        return params_;
    }
    
    // Repair defects in an image
    void repairDefects(ImageBuffer& image, const std::vector<Component>& defects) {
        if (!params_.enable || !image.isValid() || defects.empty()) {
            return;
        }
        
        // Make a copy of the image for sampling (to avoid contamination)
        // In a real implementation, we'd use a temporary buffer
        // For simplicity, we'll process defects in reverse order or use careful sampling
        
        for (const auto& defect : defects) {
            for (const auto& pixel : defect.pixels) {
                repairPixel(image, pixel.x, pixel.y);
            }
        }
    }

    // Repair an explicit list of defective coordinates.
    void repairCoordinates(ImageBuffer& image, const std::vector<PixelCoord>& coords) {
        if (!params_.enable || !image.isValid() || coords.empty()) {
            return;
        }

        for (const auto& coord : coords) {
            repairPixel(image, coord.x, coord.y);
        }
    }
    
    // Repair a single pixel
    void repairPixel(ImageBuffer& image, i32 x, i32 y) {
        if (!params_.enable || !image.isValid()) {
            return;
        }
        
        const i32 radius = params_.radius();
        const i32 width = image.width();
        const i32 height = image.height();
        
        // Check bounds
        if (x < radius || x >= width - radius || 
            y < radius || y >= height - radius) {
            return; // Skip border pixels
        }
        
        switch (params_.method) {
            case RepairMethod::NeighborMedian:
                repairWithNeighborMedian(image, x, y, radius);
                break;
            case RepairMethod::DirectionalMedian:
                repairWithDirectionalMedian(image, x, y, radius);
                break;
        }
    }
    
private:
    // Repair using median of neighbors (ring sampling)
    void repairWithNeighborMedian(ImageBuffer& image, i32 x, i32 y, i32 radius) {
        std::array<f32, 24> rSamples, gSamples, bSamples; // Max for 5x5 ring
        i32 sampleCount = 0;
        
        // Sample ring around pixel (excluding center)
        for (i32 dy = -radius; dy <= radius; ++dy) {
            for (i32 dx = -radius; dx <= radius; ++dx) {
                // Skip center pixel
                if (dx == 0 && dy == 0) {
                    continue;
                }
                
                // For ring sampling, we could use only the outer ring
                // For simplicity, we'll use all neighbors
                i32 nx = x + dx;
                i32 ny = y + dy;
                
                f32 r, g, b;
                image.getRGBNormalized(nx, ny, r, g, b);
                
                rSamples[sampleCount] = r;
                gSamples[sampleCount] = g;
                bSamples[sampleCount] = b;
                sampleCount++;
            }
        }
        
        if (sampleCount == 0) {
            return;
        }
        
        // Compute median for each channel
        std::sort(rSamples.begin(), rSamples.begin() + sampleCount);
        std::sort(gSamples.begin(), gSamples.begin() + sampleCount);
        std::sort(bSamples.begin(), bSamples.begin() + sampleCount);
        
        f32 medianR = rSamples[sampleCount / 2];
        f32 medianG = gSamples[sampleCount / 2];
        f32 medianB = bSamples[sampleCount / 2];
        
        // Replace pixel with median
        image.setPixelNormalized(x, y, medianR, medianG, medianB);
    }
    
    // Repair using directional median (preserve edges)
    void repairWithDirectionalMedian(ImageBuffer& image, i32 x, i32 y, i32 radius) {
        // Directions: horizontal, vertical, and two diagonals
        constexpr i32 numDirections = 4;
        constexpr std::array<std::pair<i32, i32>, 4> directions = {{
            {1, 0},   // horizontal
            {0, 1},   // vertical
            {1, 1},   // diagonal (\\)
            {1, -1}   // diagonal /
        }};
        
        std::array<std::array<f32, 5>, numDirections> rDirs, gDirs, bDirs;
        std::array<i32, numDirections> dirSampleCounts = {0};
        
        // Sample along each direction
        for (i32 dirIdx = 0; dirIdx < numDirections; ++dirIdx) {
            auto [dx, dy] = directions[dirIdx];
            
            // Sample in both positive and negative directions
            for (i32 step = -radius; step <= radius; ++step) {
                if (step == 0) continue; // Skip center
                
                i32 nx = x + dx * step;
                i32 ny = y + dy * step;
                
                // Check bounds
                if (nx >= 0 && nx < image.width() && ny >= 0 && ny < image.height()) {
                    f32 r, g, b;
                    image.getRGBNormalized(nx, ny, r, g, b);
                    
                    rDirs[dirIdx][dirSampleCounts[dirIdx]] = r;
                    gDirs[dirIdx][dirSampleCounts[dirIdx]] = g;
                    bDirs[dirIdx][dirSampleCounts[dirIdx]] = b;
                    dirSampleCounts[dirIdx]++;
                }
            }
        }
        
        // Find direction with minimum variance (most consistent)
        i32 bestDir = 0;
        f32 minVariance = std::numeric_limits<f32>::max();
        
        for (i32 dirIdx = 0; dirIdx < numDirections; ++dirIdx) {
            if (dirSampleCounts[dirIdx] < 2) continue;
            
            // Compute variance for this direction
            f32 meanR = 0.0f, meanG = 0.0f, meanB = 0.0f;
            
            for (i32 i = 0; i < dirSampleCounts[dirIdx]; ++i) {
                meanR += rDirs[dirIdx][i];
                meanG += gDirs[dirIdx][i];
                meanB += bDirs[dirIdx][i];
            }
            
            meanR /= dirSampleCounts[dirIdx];
            meanG /= dirSampleCounts[dirIdx];
            meanB /= dirSampleCounts[dirIdx];
            
            f32 variance = 0.0f;
            for (i32 i = 0; i < dirSampleCounts[dirIdx]; ++i) {
                f32 diffR = rDirs[dirIdx][i] - meanR;
                f32 diffG = gDirs[dirIdx][i] - meanG;
                f32 diffB = bDirs[dirIdx][i] - meanB;
                variance += diffR * diffR + diffG * diffG + diffB * diffB;
            }
            
            if (variance < minVariance) {
                minVariance = variance;
                bestDir = dirIdx;
            }
        }
        
        // Use median of best direction
        if (dirSampleCounts[bestDir] > 0) {
            std::sort(rDirs[bestDir].begin(), rDirs[bestDir].begin() + dirSampleCounts[bestDir]);
            std::sort(gDirs[bestDir].begin(), gDirs[bestDir].begin() + dirSampleCounts[bestDir]);
            std::sort(bDirs[bestDir].begin(), bDirs[bestDir].begin() + dirSampleCounts[bestDir]);
            
            f32 medianR = rDirs[bestDir][dirSampleCounts[bestDir] / 2];
            f32 medianG = gDirs[bestDir][dirSampleCounts[bestDir] / 2];
            f32 medianB = bDirs[bestDir][dirSampleCounts[bestDir] / 2];
            
            image.setPixelNormalized(x, y, medianR, medianG, medianB);
        } else {
            // Fall back to neighbor median
            repairWithNeighborMedian(image, x, y, radius);
        }
    }
    
private:
    RepairParams params_;
};

} // namespace DeadPixelQC
