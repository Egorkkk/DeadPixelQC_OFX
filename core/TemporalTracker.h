#pragma once

#include "Types.h"
#include "SpatialDetector.h"
#include <unordered_map>
#include <vector>
#include <mutex>
#include <atomic>
#include <cmath>

namespace DeadPixelQC {

// Temporal tracking parameters
struct TemporalTrackerParams {
    enum class Mode {
        Off,            // No temporal tracking
        SequentialOnly  // Only track when frames are sequential
    };
    
    Mode mode = Mode::Off;
    i32 stuckWindowFrames = 60;      // N_stuck (default 60)
    f32 stuckMinFraction = 0.95f;    // minFraction (default 0.95)
    i32 maxGapFrames = 2;            // maxGap (default 1-2)
    
    bool validate() const {
        return stuckWindowFrames > 0 &&
               stuckMinFraction >= 0.0f && stuckMinFraction <= 1.0f &&
               maxGapFrames >= 0;
    }
};

// Track for a single coordinate or component
struct TemporalTrack {
    i32 hits = 0;           // Number of frames where pixel was detected
    i32 frames = 0;         // Total frames in window
    i32 lastSeenFrame = -1; // Last frame index where detected
    i32 maxGap = 0;         // Maximum gap between detections
    f32 score = 0.0f;       // hits/frames
    
    // Update track with new detection
    void update(i32 currentFrame, bool detected) {
        frames++;
        
        if (detected) {
            hits++;
            
            // Update gap statistics
            if (lastSeenFrame >= 0) {
                i32 gap = currentFrame - lastSeenFrame - 1;
                if (gap > maxGap) {
                    maxGap = gap;
                }
            }
            
            lastSeenFrame = currentFrame;
        }
        
        // Update score
        if (frames > 0) {
            score = static_cast<f32>(hits) / frames;
        }
    }
    
    // Check if this track represents a stuck pixel
    bool isStuck(const TemporalTrackerParams& params) const {
        if (frames < params.stuckWindowFrames) {
            return false; // Not enough frames in window
        }
        
        return score >= params.stuckMinFraction && 
               maxGap <= params.maxGapFrames;
    }
    
    // Age the track (remove old frames from window)
    void age(i32 framesToRemove) {
        if (framesToRemove <= 0) return;
        
        // Simple aging: reduce window size
        frames = std::max(0, frames - framesToRemove);
        hits = std::max(0, hits - framesToRemove);
        
        if (frames > 0) {
            score = static_cast<f32>(hits) / frames;
        } else {
            score = 0.0f;
        }
    }
};

// Main temporal tracker class
class TemporalTracker {
public:
    TemporalTracker() = default;
    
    explicit TemporalTracker(const TemporalTrackerParams& params)
        : params_(params), lastFrameIndex_(-1), sequentialViolations_(0) {}
    
    // Set parameters
    void setParams(const TemporalTrackerParams& params) {
        std::lock_guard<std::mutex> lock(mutex_);
        params_ = params;
    }
    
    // Get current parameters
    TemporalTrackerParams getParams() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return params_;
    }
    
    // Process frame detection results with temporal tracking
    FrameDetection processFrame(const FrameDetection& spatialResult) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        FrameDetection result = spatialResult;
        
        if (params_.mode == TemporalTrackerParams::Mode::Off) {
            // No temporal tracking, just copy spatial results
            result.confirmed = result.candidates;
            return result;
        }
        
        i32 currentFrame = spatialResult.frameIndex;
        
        // Check for sequential violations
        if (params_.mode == TemporalTrackerParams::Mode::SequentialOnly) {
            if (lastFrameIndex_ >= 0 && currentFrame != lastFrameIndex_ + 1) {
                // Non-sequential frame detected
                sequentialViolations_++;
                
                // Reset temporal state for safety
                resetTemporalState();
                
                // Fall back to spatial-only for this frame
                result.confirmed = result.candidates;
                lastFrameIndex_ = currentFrame;
                return result;
            }
        }
        
        // Update last frame index
        lastFrameIndex_ = currentFrame;
        
        // Update tracks for each candidate component
        for (const auto& comp : spatialResult.candidates) {
            // Use centroid as key (rounded to integer coordinates)
            i32 keyX = static_cast<i32>(std::round(comp.centroidX));
            i32 keyY = static_cast<i32>(std::round(comp.centroidY));
            u64 key = (static_cast<u64>(keyX) << 32) | static_cast<u64>(keyY);
            
            auto& track = tracks_[key];
            track.update(currentFrame, true);
            
            // Age other tracks (simulate sliding window)
            // In a real implementation, we'd maintain a proper sliding window
            // For simplicity, we age tracks that haven't been updated
            if (track.frames > params_.stuckWindowFrames) {
                track.age(1);
            }
        }
        
        // Also update tracks for coordinates not detected in this frame
        // (mark as not detected to maintain accurate hit counts)
        for (auto& kv : tracks_) {
            bool detectedThisFrame = false;
            u64 key = kv.first;
            
            // Check if this key corresponds to any candidate
            for (const auto& comp : spatialResult.candidates) {
                i32 keyX = static_cast<i32>(std::round(comp.centroidX));
                i32 keyY = static_cast<i32>(std::round(comp.centroidY));
                u64 compKey = (static_cast<u64>(keyX) << 32) | static_cast<u64>(keyY);
                
                if (compKey == key) {
                    detectedThisFrame = true;
                    break;
                }
            }
            
            if (!detectedThisFrame) {
                kv.second.update(currentFrame, false);
                
                // Age if window is full
                if (kv.second.frames > params_.stuckWindowFrames) {
                    kv.second.age(1);
                }
            }
        }
        
        // Find confirmed stuck pixels
        for (const auto& comp : spatialResult.candidates) {
            i32 keyX = static_cast<i32>(std::round(comp.centroidX));
            i32 keyY = static_cast<i32>(std::round(comp.centroidY));
            u64 key = (static_cast<u64>(keyX) << 32) | static_cast<u64>(keyY);
            
            auto it = tracks_.find(key);
            if (it != tracks_.end() && it->second.isStuck(params_)) {
                result.confirmed.push_back(comp);
            }
        }
        
        // Clean up old tracks (optional optimization)
        cleanupOldTracks(currentFrame);
        
        return result;
    }
    
    // Reset temporal state (e.g., when scrubbing or scene change)
    void resetTemporalState() {
        std::lock_guard<std::mutex> lock(mutex_);
        tracks_.clear();
        lastFrameIndex_ = -1;
    }
    
    // Get number of sequential violations
    i32 getSequentialViolations() const {
        return sequentialViolations_;
    }
    
    // Get number of active tracks
    size_t getTrackCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return tracks_.size();
    }
    
private:
    // Clean up tracks that haven't been seen in a while
    void cleanupOldTracks(i32 currentFrame) {
        const i32 MAX_AGE = params_.stuckWindowFrames * 2;
        
        auto it = tracks_.begin();
        while (it != tracks_.end()) {
            const auto& track = it->second;
            
            // Remove if too old or never confirmed
            if (track.lastSeenFrame < 0 || 
                (currentFrame - track.lastSeenFrame) > MAX_AGE) {
                it = tracks_.erase(it);
            } else {
                ++it;
            }
        }
    }
    
private:
    mutable std::mutex mutex_;
    TemporalTrackerParams params_;
    std::unordered_map<u64, TemporalTrack> tracks_;
    i32 lastFrameIndex_;
    std::atomic<i32> sequentialViolations_{0};
};

} // namespace DeadPixelQC