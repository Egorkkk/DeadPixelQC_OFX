#include "SyntheticGenerator.h"
#include "../core/ColorMetrics.h"
#include "../core/SpatialDetector.h"
#include "../core/TemporalTracker.h"
#include "../core/Fixer.h"
#include <iostream>
#include <cassert>
#include <cmath>

using namespace DeadPixelQC;

// Test ColorMetrics
void testColorMetrics() {
    std::cout << "Testing ColorMetrics...\n";
    
    // Test luma computation
    f32 luma = ColorUtils::computeLuma(0.5f, 0.5f, 0.5f);
    assert(std::abs(luma - 0.5f) < 0.001f);
    
    // Test whiteness metric
    f32 whiteness = ColorUtils::computeWhitenessFast(0.9f, 0.9f, 0.9f);
    assert(whiteness < 0.01f); // Should be near zero for white
    
    whiteness = ColorUtils::computeWhitenessFast(1.0f, 0.0f, 0.0f);
    assert(whiteness > 1.0f); // Should be large for colored pixel
    
    // Test saturation
    f32 saturation = ColorUtils::computeSaturationFast(0.9f, 0.9f, 0.9f);
    assert(saturation < 0.01f); // Near zero for white
    
    saturation = ColorUtils::computeSaturationFast(1.0f, 0.0f, 0.0f);
    assert(saturation > 0.9f); // High for saturated color
    
    std::cout << "  ColorMetrics tests passed!\n";
}

// Test SpatialDetector with synthetic images
void testSpatialDetector() {
    std::cout << "Testing SpatialDetector...\n";
    
    const i32 width = 64;
    const i32 height = 64;
    
    // Create test with single stuck pixel
    auto testConfig = SyntheticGenerator::createSingleStuckTest(32, 32, 0.98f);
    auto imageData = SyntheticGenerator::generateTestImage(width, height, testConfig);
    auto imageBuffer = SyntheticGenerator::createImageBuffer(imageData, width, height);
    
    // Configure detector
    SpatialDetectorParams params;
    params.colorGate.lumaThreshold = 0.95f;
    params.colorGate.whitenessThreshold = 0.1f;
    params.colorGate.useSaturation = false;
    params.contrastGate.zScoreThreshold = 8.0f;
    params.contrastGate.neighborhood = RobustContrastParams::Neighborhood::ThreeByThree;
    params.maxClusterArea = 4;
    
    SpatialDetector detector(params);
    auto detection = detector.processFrame(imageBuffer, 0);
    
    // Should detect the stuck pixel
    assert(!detection.candidates.empty());
    
    // Generate ground truth
    auto groundTruth = SyntheticGenerator::generateGroundTruthMask(width, height, testConfig.stuckPixels);
    auto validation = SyntheticGenerator::validateDetection(detection.candidates, groundTruth, width, height);
    
    std::cout << "  Single stuck pixel: ";
    std::cout << "TP=" << validation.truePositives << ", ";
    std::cout << "FP=" << validation.falsePositives << ", ";
    std::cout << "FN=" << validation.falseNegatives << "\n";
    
    assert(validation.truePositives >= 1);
    assert(validation.falsePositives == 0);
    
    // Test with cluster
    testConfig = SyntheticGenerator::createClusterTest(32, 32, 1); // 3x3 cluster
    imageData = SyntheticGenerator::generateTestImage(width, height, testConfig);
    imageBuffer = SyntheticGenerator::createImageBuffer(imageData, width, height);
    
    detection = detector.processFrame(imageBuffer, 0);
    
    // Should detect cluster (area <= 4 means only center 2x2 might pass)
    std::cout << "  Cluster detection: " << detection.candidates.size() << " components\n";
    
    // Test false positive rejection (moving highlight)
    testConfig = SyntheticGenerator::createMovingHighlightTest(32, 32, 0.96f);
    imageData = SyntheticGenerator::generateTestImage(width, height, testConfig);
    imageBuffer = SyntheticGenerator::createImageBuffer(imageData, width, height);
    
    detection = detector.processFrame(imageBuffer, 0);
    
    // Should NOT detect moving highlight (should be filtered by contrast gate)
    std::cout << "  Moving highlight: " << detection.candidates.size() << " candidates (should be 0)\n";
    assert(detection.candidates.empty());
    
    std::cout << "  SpatialDetector tests passed!\n";
}

// Test TemporalTracker
void testTemporalTracker() {
    std::cout << "Testing TemporalTracker...\n";
    
    const i32 width = 64;
    const i32 height = 64;
    const i32 numFrames = 30;
    
    // Create sequence with stuck pixel
    SequenceConfig seqConfig;
    seqConfig.stuckPositions.push_back({32, 32});
    seqConfig.stuckBrightness = 0.98f;
    seqConfig.numMovingHighlights = 2;
    
    auto sequence = SyntheticGenerator::generateTestSequence(width, height, numFrames, seqConfig);
    
    // Configure spatial detector
    SpatialDetectorParams spatialParams;
    spatialParams.colorGate.lumaThreshold = 0.95f;
    spatialParams.colorGate.whitenessThreshold = 0.1f;
    spatialParams.contrastGate.zScoreThreshold = 8.0f;
    
    SpatialDetector spatialDetector(spatialParams);
    
    // Configure temporal tracker
    TemporalTrackerParams tempParams;
    tempParams.mode = TemporalTrackerParams::Mode::SequentialOnly;
    tempParams.stuckWindowFrames = 10;
    tempParams.stuckMinFraction = 0.9f;
    tempParams.maxGapFrames = 2;
    
    TemporalTracker temporalTracker(tempParams);
    
    // Process sequence
    std::vector<Component> allConfirmed;
    
    for (i32 frame = 0; frame < numFrames; ++frame) {
        auto imageBuffer = SyntheticGenerator::createImageBuffer(sequence[frame], width, height);
        auto spatialResult = spatialDetector.processFrame(imageBuffer, frame);
        auto temporalResult = temporalTracker.processFrame(spatialResult);
        
        // Collect confirmed defects
        allConfirmed.insert(allConfirmed.end(), 
                           temporalResult.confirmed.begin(), 
                           temporalResult.confirmed.end());
    }
    
    // Should have confirmed the stuck pixel after enough frames
    std::cout << "  Total confirmed defects: " << allConfirmed.size() << "\n";
    
    // Generate ground truth for stuck pixel
    std::vector<StuckPixel> stuckPixels = {{32, 32, 0.98f}};
    auto groundTruth = SyntheticGenerator::generateGroundTruthMask(width, height, stuckPixels);
    auto validation = SyntheticGenerator::validateDetection(allConfirmed, groundTruth, width, height);
    
    std::cout << "  Temporal confirmation: ";
    std::cout << "TP=" << validation.truePositives << ", ";
    std::cout << "FP=" << validation.falsePositives << ", ";
    std::cout << "Recall=" << validation.recall << "\n";
    
    // Should have high recall for stuck pixel
    assert(validation.recall > 0.5f);
    
    std::cout << "  TemporalTracker tests passed!\n";
}

// Test Fixer
void testFixer() {
    std::cout << "Testing Fixer...\n";
    
    const i32 width = 64;
    const i32 height = 64;
    
    // Create test with stuck pixel
    auto testConfig = SyntheticGenerator::createSingleStuckTest(32, 32, 1.0f);
    auto imageData = SyntheticGenerator::generateTestImage(width, height, testConfig);
    auto imageBuffer = SyntheticGenerator::createImageBuffer(imageData, width, height);
    
    // Create defect component
    Component defect;
    defect.pixels.push_back({32, 32});
    defect.computeProperties();
    
    std::vector<Component> defects = {defect};
    
    // Test neighbor median repair
    RepairParams repairParams;
    repairParams.enable = true;
    repairParams.method = RepairMethod::NeighborMedian;
    repairParams.kernelSize = 3;
    
    Fixer fixer(repairParams);
    
    // Make a copy of the image for repair
    std::vector<u8> imageCopy = imageData;
    ImageBuffer imageBufferCopy(imageCopy.data(), PixelFormat::RGB8, width, height);
    
    // Repair defect
    fixer.repairDefects(imageBufferCopy, defects);
    
    // Check that pixel was repaired (should be darker than original)
    f32 r, g, b;
    imageBufferCopy.getRGBNormalized(32, 32, r, g, b);
    f32 repairedLuma = ColorUtils::computeLuma(r, g, b);
    
    std::cout << "  Original brightness: 1.0, Repaired brightness: " << repairedLuma << "\n";
    assert(repairedLuma < 0.9f); // Should be significantly darker
    
    // Test directional median repair
    repairParams.method = RepairMethod::DirectionalMedian;
    fixer.setParams(repairParams);
    
    imageCopy = imageData;
    ImageBuffer imageBufferCopy2(imageCopy.data(), PixelFormat::RGB8, width, height);
    fixer.repairDefects(imageBufferCopy2, defects);
    
    imageBufferCopy2.getRGBNormalized(32, 32, r, g, b);
    f32 directionalLuma = ColorUtils::computeLuma(r, g, b);
    
    std::cout << "  Directional repair brightness: " << directionalLuma << "\n";
    assert(directionalLuma < 0.9f);
    
    std::cout << "  Fixer tests passed!\n";
}

// Main test runner
int main() {
    std::cout << "=== DeadPixelQC Unit Tests ===\n\n";
    
    try {
        testColorMetrics();
        std::cout << "\n";
        
        testSpatialDetector();
        std::cout << "\n";
        
        testTemporalTracker();
        std::cout << "\n";
        
        testFixer();
        std::cout << "\n";
        
        std::cout << "=== All tests passed! ===\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << "\n";
        return 1;
    } catch (...) {
        std::cerr << "Test failed with unknown exception\n";
        return 1;
    }
}