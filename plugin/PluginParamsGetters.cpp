#include "PluginParams.h"
#include "OfxUtil.h"
#include "DebugLog.h"

namespace DeadPixelQC_OFX {

// Get spatial detector parameters
DeadPixelQC::SpatialDetectorParams getSpatialParams(OfxParamSetHandle paramSet, double time) {
    DeadPixelQC::SpatialDetectorParams params;
    
    // Get parameters with default values
    double lumaThreshold = DEFAULT_LUMA_THRESHOLD;
    double whitenessThreshold = DEFAULT_WHITENESS_THRESHOLD;
    double robustZ = DEFAULT_ROBUST_Z_3X3;
    int maxClusterArea = DEFAULT_MAX_CLUSTER_AREA;
    
    // Try to get actual values
    OfxUtil::getParamDoubleAtTime(paramSet, PARAM_LUMA_THRESHOLD, time, lumaThreshold);
    OfxUtil::getParamDoubleAtTime(paramSet, PARAM_WHITENESS_THRESHOLD, time, whitenessThreshold);
    OfxUtil::getParamDoubleAtTime(paramSet, PARAM_ROBUST_Z, time, robustZ);
    OfxUtil::getParamIntAtTime(paramSet, PARAM_MAX_CLUSTER_AREA, time, maxClusterArea);
    
    params.colorGate.lumaThreshold = static_cast<DeadPixelQC::f32>(lumaThreshold);
    params.colorGate.whitenessThreshold = static_cast<DeadPixelQC::f32>(whitenessThreshold);
    params.colorGate.useSaturation = false;
    
    // Get neighborhood
    int neighborhood = 0;
    OfxUtil::getParamIntAtTime(paramSet, PARAM_NEIGHBORHOOD, time, neighborhood);
    params.contrastGate.neighborhood = (neighborhood == 0) ? 
        DeadPixelQC::RobustContrastParams::Neighborhood::ThreeByThree : 
        DeadPixelQC::RobustContrastParams::Neighborhood::FiveByFive;
    params.contrastGate.zScoreThreshold = static_cast<DeadPixelQC::f32>(robustZ);
    
    params.maxClusterArea = maxClusterArea;
    
    return params;
}

// Get temporal tracker parameters
DeadPixelQC::TemporalTrackerParams getTemporalParams(OfxParamSetHandle paramSet, double time) {
    DeadPixelQC::TemporalTrackerParams params;
    
    int temporalMode = 0;
    OfxUtil::getParamIntAtTime(paramSet, PARAM_TEMPORAL_MODE, time, temporalMode);
    params.mode = (temporalMode == 0) ? 
        DeadPixelQC::TemporalTrackerParams::Mode::Off : 
        DeadPixelQC::TemporalTrackerParams::Mode::SequentialOnly;
    
    OfxUtil::getParamIntAtTime(paramSet, PARAM_STUCK_WINDOW_FRAMES, time, params.stuckWindowFrames);
    
    double stuckMinFraction = DEFAULT_STUCK_MIN_FRACTION;
    OfxUtil::getParamDoubleAtTime(paramSet, PARAM_STUCK_MIN_FRACTION, time, stuckMinFraction);
    params.stuckMinFraction = static_cast<DeadPixelQC::f32>(stuckMinFraction);
    
    OfxUtil::getParamIntAtTime(paramSet, PARAM_MAX_GAP_FRAMES, time, params.maxGapFrames);
    
    return params;
}

// Get repair parameters
DeadPixelQC::RepairParams getRepairParams(OfxParamSetHandle paramSet, double time) {
    DeadPixelQC::RepairParams params;
    
    OfxUtil::getParamBoolAtTime(paramSet, PARAM_REPAIR_ENABLE, time, params.enable);
    
    int repairMethod = 0;
    OfxUtil::getParamIntAtTime(paramSet, PARAM_REPAIR_METHOD, time, repairMethod);
    params.method = (repairMethod == 0) ? 
        DeadPixelQC::RepairMethod::NeighborMedian : 
        DeadPixelQC::RepairMethod::DirectionalMedian;
    
    // Default kernel size
    params.kernelSize = 3;
    
    return params;
}

// Get view mode
int getViewMode(OfxParamSetHandle paramSet, double time) {
    int viewMode = 0;
    OfxUtil::getParamIntAtTime(paramSet, PARAM_VIEW_MODE, time, viewMode);
    return viewMode;
}

// Check if effect is enabled
bool isEnabled(OfxParamSetHandle paramSet, double time) {
    bool enabled = true;
    OfxUtil::getParamBoolAtTime(paramSet, PARAM_ENABLE, time, enabled);
    return enabled;
}

} // namespace DeadPixelQC_OFX