#include "OfxSimple.h"
#include "PluginParamsSimple.h"
#include "../core/SpatialDetector.h"
#include "../core/TemporalTracker.h"
#include "../core/Fixer.h"
#include <memory>
#include <vector>

using namespace DeadPixelQC;

// Global plugin instance counter
static int gPluginInstanceCount = 0;

// Plugin instance data
struct DeadPixelQCInstance {
    std::unique_ptr<SpatialDetector> spatialDetector;
    std::unique_ptr<TemporalTracker> temporalTracker;
    std::unique_ptr<Fixer> fixer;
    PluginParams params;
    i32 lastFrameIndex = -1;
    bool temporalResetNeeded = false;
    
    DeadPixelQCInstance() {
        // Initialize with default parameters
        SpatialDetectorParams spatialParams;
        spatialParams.colorGate.lumaThreshold = 0.98f;
        spatialParams.colorGate.whitenessThreshold = 0.05f;
        spatialParams.contrastGate.zScoreThreshold = 10.0f;
        spatialParams.contrastGate.neighborhood = RobustContrastParams::Neighborhood::ThreeByThree;
        spatialParams.maxClusterArea = 4;
        
        TemporalTrackerParams tempParams;
        tempParams.mode = TemporalTrackerParams::Mode::SequentialOnly;
        tempParams.stuckWindowFrames = 60;
        tempParams.stuckMinFraction = 0.95f;
        tempParams.maxGapFrames = 2;
        
        RepairParams repairParams;
        repairParams.enable = false;
        repairParams.method = RepairMethod::NeighborMedian;
        repairParams.kernelSize = 3;
        
        spatialDetector = std::make_unique<SpatialDetector>(spatialParams);
        temporalTracker = std::make_unique<TemporalTracker>(tempParams);
        fixer = std::make_unique<Fixer>(repairParams);
    }
};

// Plugin entry point (simplified)
extern "C" __declspec(dllexport) const char* OfxPluginMain() {
    gPluginInstanceCount++;
    return "DeadPixelQC Plugin v1.0";
}

// Create plugin instance
extern "C" __declspec(dllexport) void* createInstance() {
    try {
        return new DeadPixelQCInstance();
    } catch (...) {
        return nullptr;
    }
}

// Destroy plugin instance
extern "C" __declspec(dllexport) void destroyInstance(void* instance) {
    if (instance) {
        delete static_cast<DeadPixelQCInstance*>(instance);
        gPluginInstanceCount--;
    }
}

// Process frame (simplified version)
extern "C" __declspec(dllexport) bool processFrame(void* instance, 
                                                   const unsigned char* input, 
                                                   unsigned char* output,
                                                   int width, int height,
                                                   int frameIndex) {
    if (!instance || !input || !output || width <= 0 || height <= 0) {
        return false;
    }
    
    try {
        DeadPixelQCInstance* plugin = static_cast<DeadPixelQCInstance*>(instance);
        
        // Create image buffer from input
        ImageBuffer inputBuffer(const_cast<void*>(static_cast<const void*>(input)), 
                               PixelFormat::RGB8, width, height);
        
        // Check for temporal reset
        if (frameIndex <= plugin->lastFrameIndex) {
            plugin->temporalResetNeeded = true;
        }
        plugin->lastFrameIndex = frameIndex;
        
        // Process frame
        FrameDetection detection;
        
        if (plugin->params.enable) {
            // Update detector parameters from plugin params
            SpatialDetectorParams spatialParams;
            spatialParams.colorGate.lumaThreshold = plugin->params.lumaThreshold;
            spatialParams.colorGate.whitenessThreshold = plugin->params.whitenessThreshold;
            spatialParams.contrastGate.zScoreThreshold = plugin->params.robustZ;
            spatialParams.contrastGate.neighborhood = plugin->params.neighborhood == 0 ? 
                RobustContrastParams::Neighborhood::ThreeByThree : 
                RobustContrastParams::Neighborhood::FiveByFive;
            spatialParams.maxClusterArea = plugin->params.maxClusterArea;
            
            plugin->spatialDetector->setParams(spatialParams);
            
            // Process spatial detection
            detection = plugin->spatialDetector->processFrame(inputBuffer, frameIndex);
            
            // Apply temporal tracking if enabled
            if (plugin->params.temporalMode != 0) { // Not Off
                if (plugin->temporalResetNeeded) {
                    // Reset temporal tracker
                    TemporalTrackerParams tempParams;
                    tempParams.mode = TemporalTrackerParams::Mode::SequentialOnly;
                    tempParams.stuckWindowFrames = plugin->params.stuckWindowFrames;
                    tempParams.stuckMinFraction = plugin->params.stuckMinFraction;
                    tempParams.maxGapFrames = plugin->params.maxGapFrames;
                    
                    plugin->temporalTracker = std::make_unique<TemporalTracker>(tempParams);
                    plugin->temporalResetNeeded = false;
                }
                
                detection = plugin->temporalTracker->processFrame(detection);
            }
        }
        
        // Create output image
        ImageBuffer outputBuffer(output, PixelFormat::RGB8, width, height);
        
        // Apply appropriate output mode
        switch (plugin->params.viewMode) {
            case 0: // Output
                // Copy input to output
                for (int y = 0; y < height; ++y) {
                    for (int x = 0; x < width; ++x) {
                        f32 r, g, b;
                        inputBuffer.getRGBNormalized(x, y, r, g, b);
                        outputBuffer.setPixelNormalized(x, y, r, g, b);
                    }
                }
                
                // Apply repair if enabled
                if (plugin->params.repairEnable && !detection.confirmed.empty()) {
                    RepairParams repairParams;
                    repairParams.enable = true;
                    repairParams.method = plugin->params.repairMethod == 0 ? 
                        RepairMethod::NeighborMedian : RepairMethod::DirectionalMedian;
                    repairParams.kernelSize = 3;
                    
                    plugin->fixer->setParams(repairParams);
                    plugin->fixer->repairDefects(outputBuffer, detection.confirmed);
                }
                break;
                
            case 1: // CandidatesOverlay
                // Copy input to output
                for (int y = 0; y < height; ++y) {
                    for (int x = 0; x < width; ++x) {
                        f32 r, g, b;
                        inputBuffer.getRGBNormalized(x, y, r, g, b);
                        outputBuffer.setPixelNormalized(x, y, r, g, b);
                    }
                }
                // Apply yellow overlay for candidates
                for (const auto& comp : detection.candidates) {
                    for (const auto& pixel : comp.pixels) {
                        if (pixel.x >= 0 && pixel.x < width && pixel.y >= 0 && pixel.y < height) {
                            f32 r, g, b;
                            outputBuffer.getRGBNormalized(pixel.x, pixel.y, r, g, b);
                            // Yellow blend
                            const f32 alpha = 0.7f;
                            r = r * (1.0f - alpha) + 1.0f * alpha; // Red
                            g = g * (1.0f - alpha) + 1.0f * alpha; // Green
                            b = b * (1.0f - alpha) + 0.0f * alpha; // Blue
                            outputBuffer.setPixelNormalized(pixel.x, pixel.y, r, g, b);
                        }
                    }
                }
                break;
                
            case 2: // ConfirmedOverlay
                // Copy input to output
                for (int y = 0; y < height; ++y) {
                    for (int x = 0; x < width; ++x) {
                        f32 r, g, b;
                        inputBuffer.getRGBNormalized(x, y, r, g, b);
                        outputBuffer.setPixelNormalized(x, y, r, g, b);
                    }
                }
                // Apply red overlay for confirmed defects
                for (const auto& comp : detection.confirmed) {
                    for (const auto& pixel : comp.pixels) {
                        if (pixel.x >= 0 && pixel.x < width && pixel.y >= 0 && pixel.y < height) {
                            f32 r, g, b;
                            outputBuffer.getRGBNormalized(pixel.x, pixel.y, r, g, b);
                            // Red blend
                            const f32 alpha = 0.7f;
                            r = r * (1.0f - alpha) + 1.0f * alpha; // Red
                            g = g * (1.0f - alpha) + 0.0f * alpha; // Green
                            b = b * (1.0f - alpha) + 0.0f * alpha; // Blue
                            outputBuffer.setPixelNormalized(pixel.x, pixel.y, r, g, b);
                        }
                    }
                }
                break;
                
            case 3: // MaskOnly
                // Create mask image
                for (int y = 0; y < height; ++y) {
                    for (int x = 0; x < width; ++x) {
                        outputBuffer.setPixelNormalized(x, y, 0.0f, 0.0f, 0.0f); // Black background
                    }
                }
                // White pixels for confirmed defects
                for (const auto& comp : detection.confirmed) {
                    for (const auto& pixel : comp.pixels) {
                        if (pixel.x >= 0 && pixel.x < width && pixel.y >= 0 && pixel.y < height) {
                            outputBuffer.setPixelNormalized(pixel.x, pixel.y, 1.0f, 1.0f, 1.0f); // White
                        }
                    }
                }
                break;
        }
        
        return true;
        
    } catch (...) {
        return false;
    }
}

// Get parameter value
extern "C" __declspec(dllexport) bool getParameter(void* instance, int paramId, void* value) {
    if (!instance || !value) return false;
    
    DeadPixelQCInstance* plugin = static_cast<DeadPixelQCInstance*>(instance);
    
    switch (paramId) {
        case 0: // Enable
            *static_cast<bool*>(value) = plugin->params.enable;
            break;
        case 1: // ViewMode
            *static_cast<int*>(value) = plugin->params.viewMode;
            break;
        case 2: // LumaThreshold
            *static_cast<float*>(value) = plugin->params.lumaThreshold;
            break;
        case 3: // WhitenessThreshold
            *static_cast<float*>(value) = plugin->params.whitenessThreshold;
            break;
        case 4: // Neighborhood
            *static_cast<int*>(value) = plugin->params.neighborhood;
            break;
        case 5: // RobustZ
            *static_cast<float*>(value) = plugin->params.robustZ;
            break;
        case 6: // MaxClusterArea
            *static_cast<int*>(value) = plugin->params.maxClusterArea;
            break;
        case 7: // TemporalMode
            *static_cast<int*>(value) = plugin->params.temporalMode;
            break;
        case 8: // StuckWindowFrames
            *static_cast<int*>(value) = plugin->params.stuckWindowFrames;
            break;
        case 9: // StuckMinFraction
            *static_cast<float*>(value) = plugin->params.stuckMinFraction;
            break;
        case 10: // MaxGapFrames
            *static_cast<int*>(value) = plugin->params.maxGapFrames;
            break;
        case 11: // RepairEnable
            *static_cast<bool*>(value) = plugin->params.repairEnable;
            break;
        case 12: // RepairMethod
            *static_cast<int*>(value) = plugin->params.repairMethod;
            break;
        default:
            return false;
    }
    
    return true;
}

// Set parameter value
extern "C" __declspec(dllexport) bool setParameter(void* instance, int paramId, const void* value) {
    if (!instance || !value) return false;
    
    DeadPixelQCInstance* plugin = static_cast<DeadPixelQCInstance*>(instance);
    
    switch (paramId) {
        case 0: // Enable
            plugin->params.enable = *static_cast<const bool*>(value);
            break;
        case 1: // ViewMode
            plugin->params.viewMode = *static_cast<const int*>(value);
            break;
        case 2: // LumaThreshold
            plugin->params.lumaThreshold = *static_cast<const float*>(value);
            break;
        case 3: // WhitenessThreshold
            plugin->params.whitenessThreshold = *static_cast<const float*>(value);
            break;
        case 4: // Neighborhood
            plugin->params.neighborhood = *static_cast<const int*>(value);
            break;
        case 5: // RobustZ
            plugin->params.robustZ = *static_cast<const float*>(value);
            break;
        case 6: // MaxClusterArea
            plugin->params.maxClusterArea = *static_cast<const int*>(value);
            break;
        case 7: // TemporalMode
            plugin->params.temporalMode = *static_cast<const int*>(value);
            break;
        case 8: // StuckWindowFrames
            plugin->params.stuckWindowFrames = *static_cast<const int*>(value);
            break;
        case 9: // StuckMinFraction
            plugin->params.stuckMinFraction = *static_cast<const float*>(value);
            break;
        case 10: // MaxGapFrames
            plugin->params.maxGapFrames = *static_cast<const int*>(value);
            break;
        case 11: // RepairEnable
            plugin->params.repairEnable = *static_cast<const bool*>(value);
            break;
        case 12: // RepairMethod
            plugin->params.repairMethod = *static_cast<const int*>(value);
            break;
        default:
            return false;
    }
    
    return true;
}

// Get plugin information
extern "C" __declspec(dllexport) const char* getPluginInfo() {
    return "DeadPixelQC v1.0 - Stuck Pixel Detection Plugin\n"
           "Author: Demo Implementation\n"
           "Description: Detects and optionally fixes stuck/hot pixels in video footage\n"
           "Supports: Phase 1 (Spatial), Phase 2 (Temporal), Phase 3 (Repair)";
}