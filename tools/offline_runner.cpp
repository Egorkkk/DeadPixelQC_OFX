#include "../core/SpatialDetector.h"
#include "../core/TemporalTracker.h"
#include "../core/Fixer.h"
#include "../tests/SyntheticGenerator.h"
#include <iostream>
#include <fstream>
#include <filesystem>
#include <chrono>
#include <iomanip>
#include <string>
#include <cstdlib>

#ifdef HAS_OPENCV
#include <opencv2/opencv.hpp>
#endif

using namespace DeadPixelQC;

// Command line options
struct Options {
    std::string inputPath;
    std::string outputDir = "output";
    i32 width = 1920;
    i32 height = 1080;
    i32 startFrame = 0;
    i32 endFrame = 100;
    bool useTemporal = true;
    bool repairDefects = false;
    bool saveMasks = true;
    bool saveReport = true;
    bool syntheticMode = false;
};

// Parse command line arguments
Options parseOptions(int argc, char* argv[]) {
    Options opts;
    
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        
        if (arg == "-i" && i + 1 < argc) {
            opts.inputPath = argv[++i];
        } else if (arg == "-o" && i + 1 < argc) {
            opts.outputDir = argv[++i];
        } else if (arg == "-w" && i + 1 < argc) {
            opts.width = std::stoi(argv[++i]);
        } else if (arg == "-h" && i + 1 < argc) {
            opts.height = std::stoi(argv[++i]);
        } else if (arg == "-s" && i + 1 < argc) {
            opts.startFrame = std::stoi(argv[++i]);
        } else if (arg == "-e" && i + 1 < argc) {
            opts.endFrame = std::stoi(argv[++i]);
        } else if (arg == "--no-temporal") {
            opts.useTemporal = false;
        } else if (arg == "--repair") {
            opts.repairDefects = true;
        } else if (arg == "--no-masks") {
            opts.saveMasks = false;
        } else if (arg == "--no-report") {
            opts.saveReport = false;
        } else if (arg == "--synthetic") {
            opts.syntheticMode = true;
        } else if (arg == "--help") {
            std::cout << "Usage: offline_runner [options]\n";
            std::cout << "Options:\n";
            std::cout << "  -i <path>       Input image sequence pattern or directory\n";
            std::cout << "  -o <dir>        Output directory (default: output)\n";
            std::cout << "  -w <width>      Image width (default: 1920)\n";
            std::cout << "  -h <height>     Image height (default: 1080)\n";
            std::cout << "  -s <frame>      Start frame (default: 0)\n";
            std::cout << "  -e <frame>      End frame (default: 100)\n";
            std::cout << "  --no-temporal   Disable temporal tracking\n";
            std::cout << "  --repair        Enable defect repair\n";
            std::cout << "  --no-masks      Don't save mask images\n";
            std::cout << "  --no-report     Don't save CSV report\n";
            std::cout << "  --synthetic     Generate synthetic test sequence\n";
            std::cout << "  --help          Show this help\n";
            exit(0);
        }
    }
    
    return opts;
}

// Create output directory
bool createOutputDirectory(const std::string& path) {
    try {
        std::filesystem::create_directories(path);
        std::filesystem::create_directories(path + "/masks");
        std::filesystem::create_directories(path + "/repaired");
        return true;
    } catch (...) {
        return false;
    }
}

// Save mask image
bool saveMaskImage(const std::vector<u8>& mask, i32 width, i32 height, 
                   const std::string& filename) {
#ifdef HAS_OPENCV
    cv::Mat maskMat(height, width, CV_8UC1, const_cast<u8*>(mask.data()));
    return cv::imwrite(filename, maskMat);
#else
    // Simple PGM format if OpenCV not available
    std::ofstream file(filename, std::ios::binary);
    if (!file) return false;
    
    file << "P5\n" << width << " " << height << "\n255\n";
    file.write(reinterpret_cast<const char*>(mask.data()), mask.size());
    return file.good();
#endif
}

// Save repaired image
bool saveRepairedImage(const ImageBuffer& image, const std::string& filename) {
#ifdef HAS_OPENCV
    // Convert ImageBuffer to OpenCV Mat
    i32 width = image.width();
    i32 height = image.height();
    
    if (image.format() == PixelFormat::RGB8) {
        cv::Mat rgbMat(height, width, CV_8UC3, image.data());
        cv::Mat bgrMat;
        cv::cvtColor(rgbMat, bgrMat, cv::COLOR_RGB2BGR);
        return cv::imwrite(filename, bgrMat);
    }
#endif
    return false;
}

// Process a single frame
FrameDetection processSingleFrame(const ImageBuffer& image, i32 frameIndex,
                                  SpatialDetector& spatialDetector,
                                  TemporalTracker& temporalTracker,
                                  bool useTemporal) {
    auto spatialResult = spatialDetector.processFrame(image, frameIndex);
    
    if (useTemporal) {
        return temporalTracker.processFrame(spatialResult);
    } else {
        return spatialResult;
    }
}

// Generate synthetic test sequence
std::vector<std::vector<u8>> generateSyntheticSequence(const Options& opts) {
    std::cout << "Generating synthetic test sequence...\n";
    
    SequenceConfig seqConfig;
    seqConfig.stuckPositions = {
        {opts.width / 4, opts.height / 4},
        {opts.width * 3 / 4, opts.height / 4},
        {opts.width / 2, opts.height / 2}
    };
    seqConfig.stuckBrightness = 0.98f;
    seqConfig.numMovingHighlights = 3;
    seqConfig.subtitleStartFrame = 20;
    seqConfig.subtitleEndFrame = 40;
    seqConfig.subtitles.push_back({opts.width / 4, opts.height * 3 / 4, 400, 50, 1.0f});
    
    return SyntheticGenerator::generateTestSequence(
        opts.width, opts.height, opts.endFrame - opts.startFrame, seqConfig);
}

// Main processing function
int runProcessing(const Options& opts) {
    std::cout << "DeadPixelQC Offline Runner\n";
    std::cout << "===========================\n";
    
    // Create output directory
    if (!createOutputDirectory(opts.outputDir)) {
        std::cerr << "Error: Could not create output directory: " << opts.outputDir << "\n";
        return 1;
    }
    
    // Configure detectors
    SpatialDetectorParams spatialParams;
    spatialParams.colorGate.lumaThreshold = 0.98f;
    spatialParams.colorGate.whitenessThreshold = 0.05f;
    spatialParams.contrastGate.zScoreThreshold = 10.0f;
    spatialParams.contrastGate.neighborhood = RobustContrastParams::Neighborhood::ThreeByThree;
    spatialParams.maxClusterArea = 4;
    
    TemporalTrackerParams tempParams;
    tempParams.mode = opts.useTemporal ? 
        TemporalTrackerParams::Mode::SequentialOnly : 
        TemporalTrackerParams::Mode::Off;
    tempParams.stuckWindowFrames = 60;
    tempParams.stuckMinFraction = 0.95f;
    tempParams.maxGapFrames = 2;
    
    RepairParams repairParams;
    repairParams.enable = opts.repairDefects;
    repairParams.method = RepairMethod::NeighborMedian;
    repairParams.kernelSize = 3;
    
    // Create processors
    SpatialDetector spatialDetector(spatialParams);
    TemporalTracker temporalTracker(tempParams);
    Fixer fixer(repairParams);
    
    // Open report file
    std::ofstream reportFile;
    if (opts.saveReport) {
        reportFile.open(opts.outputDir + "/report.csv");
        reportFile << "frame,candidates,confirmed,processing_time_ms\n";
    }
    
    // Generate or load sequence
    std::vector<std::vector<u8>> sequence;
    
    if (opts.syntheticMode) {
        sequence = generateSyntheticSequence(opts);
    } else if (!opts.inputPath.empty()) {
        std::cerr << "Error: Image sequence loading not implemented yet\n";
        return 1;
    } else {
        std::cerr << "Error: No input specified. Use --synthetic or provide input path\n";
        return 1;
    }
    
    // Process frames
    i32 totalCandidates = 0;
    i32 totalConfirmed = 0;
    auto totalStartTime = std::chrono::high_resolution_clock::now();
    
    for (i32 frameIdx = 0; frameIdx < sequence.size(); ++frameIdx) {
        i32 globalFrame = opts.startFrame + frameIdx;
        
        auto frameStartTime = std::chrono::high_resolution_clock::now();
        
        // Create image buffer
        auto imageBuffer = SyntheticGenerator::createImageBuffer(
            sequence[frameIdx], opts.width, opts.height);
        
        // Process frame
        auto detection = processSingleFrame(imageBuffer, globalFrame,
                                           spatialDetector, temporalTracker,
                                           opts.useTemporal);
        
        auto frameEndTime = std::chrono::high_resolution_clock::now();
        auto frameTime = std::chrono::duration_cast<std::chrono::milliseconds>(
            frameEndTime - frameStartTime).count();
        
        // Update statistics
        totalCandidates += static_cast<i32>(detection.candidates.size());
        totalConfirmed += static_cast<i32>(detection.confirmed.size());
        
        // Save mask if requested
        if (opts.saveMasks && !detection.confirmed.empty()) {
            std::vector<u8> mask(opts.width * opts.height, 0);
            for (const auto& comp : detection.confirmed) {
                for (const auto& pixel : comp.pixels) {
                    i32 idx = pixel.y * opts.width + pixel.x;
                    if (idx >= 0 && idx < mask.size()) {
                        mask[idx] = 255;
                    }
                }
            }
            
            std::string maskFilename = opts.outputDir + "/masks/frame_" + 
                                      std::to_string(globalFrame).append("_mask.png");
            saveMaskImage(mask, opts.width, opts.height, maskFilename);
        }
        
        // Repair defects if requested
        if (opts.repairDefects && !detection.confirmed.empty()) {
            // Make a copy of the image
            std::vector<u8> imageCopy = sequence[frameIdx];
            ImageBuffer repairedBuffer(imageCopy.data(), PixelFormat::RGB8, 
                                      opts.width, opts.height);
            
            // Apply repair
            fixer.repairDefects(repairedBuffer, detection.confirmed);
            
            // Save repaired image
            std::string repairedFilename = opts.outputDir + "/repaired/frame_" + 
                                          std::to_string(globalFrame).append("_repaired.png");
            saveRepairedImage(repairedBuffer, repairedFilename);
        }
        
        // Write to report
        if (reportFile.is_open()) {
            reportFile << globalFrame << ","
                      << detection.candidates.size() << ","
                      << detection.confirmed.size() << ","
                      << frameTime << "\n";
        }
        
        // Progress output
        if (frameIdx % 10 == 0) {
            std::cout << "Processed frame " << globalFrame 
                      << " (" << frameIdx + 1 << "/" << sequence.size() << ")"
                      << " - Candidates: " << detection.candidates.size()
                      << ", Confirmed: " << detection.confirmed.size()
                      << ", Time: " << frameTime << "ms\n";
        }
    }
    
    auto totalEndTime = std::chrono::high_resolution_clock::now();
    auto totalTime = std::chrono::duration_cast<std::chrono::milliseconds>(
        totalEndTime - totalStartTime).count();
    
    // Print summary
    std::cout << "\nProcessing complete!\n";
    std::cout << "====================\n";
    std::cout << "Total frames: " << sequence.size() << "\n";
    std::cout << "Total candidates: " << totalCandidates << "\n";
    std::cout << "Total confirmed defects: " << totalConfirmed << "\n";
    std::cout << "Total processing time: " << totalTime << "ms\n";
    std::cout << "Average time per frame: " << (totalTime / sequence.size()) << "ms\n";
    
    if (opts.saveReport) {
        std::cout << "Report saved to: " << opts.outputDir << "/report.csv\n";
        reportFile.close();
    }
    
    return 0;
}

int main(int argc, char* argv[]) {
    try {
        Options opts = parseOptions(argc, argv);
        return runProcessing(opts);
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    } catch (...) {
        std::cerr << "Unknown error occurred\n";
        return 1;
    }
}