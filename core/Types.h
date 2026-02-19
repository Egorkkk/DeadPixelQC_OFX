#pragma once

#include <cstdint>
#include <vector>
#include <array>
#include <cmath>

namespace DeadPixelQC {

// Basic types
using f32 = float;
using f64 = double;
using u8  = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;
using i32 = int32_t;
using i64 = int64_t;

// Image dimensions
struct ImageSize {
    i32 width = 0;
    i32 height = 0;
    
    bool operator==(const ImageSize& other) const {
        return width == other.width && height == other.height;
    }
    
    bool operator!=(const ImageSize& other) const {
        return !(*this == other);
    }
    
    i32 pixelCount() const { return width * height; }
};

// Pixel coordinates
struct PixelCoord {
    i32 x = 0;
    i32 y = 0;
    
    bool operator==(const PixelCoord& other) const {
        return x == other.x && y == other.y;
    }
    
    bool operator!=(const PixelCoord& other) const {
        return !(*this == other);
    }
    
    // For use in maps/sets
    bool operator<(const PixelCoord& other) const {
        return (y < other.y) || (y == other.y && x < other.x);
    }
};

// Bounding box
struct BBox {
    i32 x1 = 0, y1 = 0;
    i32 x2 = 0, y2 = 0; // inclusive
    
    BBox() = default;
    BBox(i32 x, i32 y, i32 w, i32 h) : x1(x), y1(y), x2(x + w - 1), y2(y + h - 1) {}
    
    i32 width() const { return x2 - x1 + 1; }
    i32 height() const { return y2 - y1 + 1; }
    i32 area() const { return width() * height(); }
    
    bool contains(i32 x, i32 y) const {
        return x >= x1 && x <= x2 && y >= y1 && y <= y2;
    }
    
    bool contains(const PixelCoord& p) const {
        return contains(p.x, p.y);
    }
};

// Connected component
struct Component {
    std::vector<PixelCoord> pixels;
    BBox bbox;
    f32 centroidX = 0.0f;
    f32 centroidY = 0.0f;
    i32 area = 0;
    
    void computeProperties() {
        if (pixels.empty()) {
            bbox = BBox();
            centroidX = centroidY = 0.0f;
            area = 0;
            return;
        }
        
        i32 minX = pixels[0].x, maxX = pixels[0].x;
        i32 minY = pixels[0].y, maxY = pixels[0].y;
        i64 sumX = 0, sumY = 0;
        
        for (const auto& p : pixels) {
            if (p.x < minX) minX = p.x;
            if (p.x > maxX) maxX = p.x;
            if (p.y < minY) minY = p.y;
            if (p.y > maxY) maxY = p.y;
            sumX += p.x;
            sumY += p.y;
        }
        
        bbox = BBox(minX, minY, maxX - minX + 1, maxY - minY + 1);
        centroidX = static_cast<f32>(sumX) / pixels.size();
        centroidY = static_cast<f32>(sumY) / pixels.size();
        area = static_cast<i32>(pixels.size());
    }
};

// Detection result for a single frame
struct FrameDetection {
    std::vector<Component> candidates;      // Phase 1 candidates
    std::vector<Component> confirmed;       // Phase 2 confirmed stuck pixels
    i32 frameIndex = -1;
    f64 timestamp = 0.0;
};

// Constants
constexpr f32 EPSILON = 1e-6f;
constexpr f32 MAX_LUMA = 1.0f;
constexpr f32 MIN_LUMA = 0.0f;

// Rec.709 luma coefficients
constexpr f32 LUMA_R = 0.2126f;
constexpr f32 LUMA_G = 0.7152f;
constexpr f32 LUMA_B = 0.0722f;

// Robust statistics constants
constexpr f32 MAD_SCALE = 1.4826f; // For normal distribution

} // namespace DeadPixelQC