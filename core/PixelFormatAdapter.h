#pragma once

#include "Types.h"
#include <cstdint>
#include <cstring>
#include <type_traits>

namespace DeadPixelQC {

// Supported pixel formats
enum class PixelFormat {
    Unknown = 0,
    RGBA8,      // 8-bit per channel, RGBA
    RGB8,       // 8-bit per channel, RGB
    RGBA16,     // 16-bit per channel, RGBA
    RGB16,      // 16-bit per channel, RGB
    RGBA32F,    // 32-bit float per channel, RGBA
    RGB32F,     // 32-bit float per channel, RGB
};

// Channel count for each format
constexpr i32 getChannelCount(PixelFormat fmt) {
    switch (fmt) {
        case PixelFormat::RGBA8:
        case PixelFormat::RGBA16:
        case PixelFormat::RGBA32F:
            return 4;
        case PixelFormat::RGB8:
        case PixelFormat::RGB16:
        case PixelFormat::RGB32F:
            return 3;
        default:
            return 0;
    }
}

// Bytes per pixel for each format
constexpr i32 getBytesPerPixel(PixelFormat fmt) {
    switch (fmt) {
        case PixelFormat::RGBA8:    return 4;
        case PixelFormat::RGB8:     return 3;
        case PixelFormat::RGBA16:   return 8;
        case PixelFormat::RGB16:    return 6;
        case PixelFormat::RGBA32F:  return 16;
        case PixelFormat::RGB32F:   return 12;
        default:                    return 0;
    }
}

// Image buffer wrapper
class ImageBuffer {
public:
    ImageBuffer() = default;
    
    ImageBuffer(const void* data, PixelFormat fmt, i32 width, i32 height, i32 rowBytes = 0)
        : data_(const_cast<void*>(data))
        , format_(fmt)
        , width_(width)
        , height_(height)
        , rowBytes_(rowBytes)
    {
        if (rowBytes_ == 0) {
            rowBytes_ = width_ * getBytesPerPixel(format_);
        }
    }
    
    // Access pixel at (x, y) as normalized float [0..1]
    void getPixelNormalized(i32 x, i32 y, f32& r, f32& g, f32& b, f32& a) const {
        if (x < 0 || x >= width_ || y < 0 || y >= height_ || !data_) {
            r = g = b = a = 0.0f;
            return;
        }
        
        const u8* pixelPtr = static_cast<const u8*>(data_) + y * rowBytes_ + x * getBytesPerPixel(format_);
        
        switch (format_) {
            case PixelFormat::RGBA8:
                r = pixelPtr[0] / 255.0f;
                g = pixelPtr[1] / 255.0f;
                b = pixelPtr[2] / 255.0f;
                a = pixelPtr[3] / 255.0f;
                break;
            case PixelFormat::RGB8:
                r = pixelPtr[0] / 255.0f;
                g = pixelPtr[1] / 255.0f;
                b = pixelPtr[2] / 255.0f;
                a = 1.0f;
                break;
            case PixelFormat::RGBA16: {
                const u16* p = reinterpret_cast<const u16*>(pixelPtr);
                r = p[0] / 65535.0f;
                g = p[1] / 65535.0f;
                b = p[2] / 65535.0f;
                a = p[3] / 65535.0f;
                break;
            }
            case PixelFormat::RGB16: {
                const u16* p = reinterpret_cast<const u16*>(pixelPtr);
                r = p[0] / 65535.0f;
                g = p[1] / 65535.0f;
                b = p[2] / 65535.0f;
                a = 1.0f;
                break;
            }
            case PixelFormat::RGBA32F: {
                const f32* p = reinterpret_cast<const f32*>(pixelPtr);
                r = p[0];
                g = p[1];
                b = p[2];
                a = p[3];
                break;
            }
            case PixelFormat::RGB32F: {
                const f32* p = reinterpret_cast<const f32*>(pixelPtr);
                r = p[0];
                g = p[1];
                b = p[2];
                a = 1.0f;
                break;
            }
            default:
                r = g = b = a = 0.0f;
                break;
        }
    }
    
    // Get only RGB normalized
    void getRGBNormalized(i32 x, i32 y, f32& r, f32& g, f32& b) const {
        f32 a;
        getPixelNormalized(x, y, r, g, b, a);
    }
    
    // Set pixel at (x, y) from normalized float [0..1]
    void setPixelNormalized(i32 x, i32 y, f32 r, f32 g, f32 b, f32 a = 1.0f) {
        if (x < 0 || x >= width_ || y < 0 || y >= height_ || !data_) {
            return;
        }
        
        u8* pixelPtr = static_cast<u8*>(data_) + y * rowBytes_ + x * getBytesPerPixel(format_);
        
        switch (format_) {
            case PixelFormat::RGBA8:
                pixelPtr[0] = static_cast<u8>(r * 255.0f + 0.5f);
                pixelPtr[1] = static_cast<u8>(g * 255.0f + 0.5f);
                pixelPtr[2] = static_cast<u8>(b * 255.0f + 0.5f);
                pixelPtr[3] = static_cast<u8>(a * 255.0f + 0.5f);
                break;
            case PixelFormat::RGB8:
                pixelPtr[0] = static_cast<u8>(r * 255.0f + 0.5f);
                pixelPtr[1] = static_cast<u8>(g * 255.0f + 0.5f);
                pixelPtr[2] = static_cast<u8>(b * 255.0f + 0.5f);
                break;
            case PixelFormat::RGBA16: {
                u16* p = reinterpret_cast<u16*>(pixelPtr);
                p[0] = static_cast<u16>(r * 65535.0f + 0.5f);
                p[1] = static_cast<u16>(g * 65535.0f + 0.5f);
                p[2] = static_cast<u16>(b * 65535.0f + 0.5f);
                p[3] = static_cast<u16>(a * 65535.0f + 0.5f);
                break;
            }
            case PixelFormat::RGB16: {
                u16* p = reinterpret_cast<u16*>(pixelPtr);
                p[0] = static_cast<u16>(r * 65535.0f + 0.5f);
                p[1] = static_cast<u16>(g * 65535.0f + 0.5f);
                p[2] = static_cast<u16>(b * 65535.0f + 0.5f);
                break;
            }
            case PixelFormat::RGBA32F: {
                f32* p = reinterpret_cast<f32*>(pixelPtr);
                p[0] = r;
                p[1] = g;
                p[2] = b;
                p[3] = a;
                break;
            }
            case PixelFormat::RGB32F: {
                f32* p = reinterpret_cast<f32*>(pixelPtr);
                p[0] = r;
                p[1] = g;
                p[2] = b;
                break;
            }
            default:
                break;
        }
    }
    
    // Getters
    const void* data() const { return data_; }
    void* data() { return data_; }
    PixelFormat format() const { return format_; }
    i32 width() const { return width_; }
    i32 height() const { return height_; }
    i32 rowBytes() const { return rowBytes_; }
    i32 channelCount() const { return getChannelCount(format_); }
    bool isValid() const { return data_ != nullptr && width_ > 0 && height_ > 0; }
    
private:
    void* data_ = nullptr;
    PixelFormat format_ = PixelFormat::Unknown;
    i32 width_ = 0;
    i32 height_ = 0;
    i32 rowBytes_ = 0;
};

// Utility functions for color conversions
namespace ColorUtils {
    
    // Compute Rec.709 luma from normalized RGB
    inline f32 computeLuma(f32 r, f32 g, f32 b) {
        return r * LUMA_R + g * LUMA_G + b * LUMA_B;
    }
    
    // Fast whiteness metric: sum of absolute differences between channels
    inline f32 computeWhitenessFast(f32 r, f32 g, f32 b) {
        return std::abs(r - g) + std::abs(g - b) + std::abs(b - r);
    }
    
    // HSV saturation estimate (fast approximation)
    inline f32 computeSaturationFast(f32 r, f32 g, f32 b) {
        f32 max = std::max(r, std::max(g, b));
        f32 min = std::min(r, std::min(g, b));
        f32 delta = max - min;
        return (max < EPSILON) ? 0.0f : delta / max;
    }
    
} // namespace ColorUtils

} // namespace DeadPixelQC