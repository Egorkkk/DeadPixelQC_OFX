#pragma once

#include "ofxImageEffect.h"
#include "ofxParam.h"
#include "../core/SpatialDetector.h"
#include "../core/TemporalTracker.h"
#include "../core/Fixer.h"

namespace DeadPixelQC_OFX {

// Parameter names
extern const char* PARAM_ENABLE;
extern const char* PARAM_VIEW_MODE;
extern const char* PARAM_LUMA_THRESHOLD;
extern const char* PARAM_WHITENESS_THRESHOLD;
extern const char* PARAM_NEIGHBORHOOD;
extern const char* PARAM_ROBUST_Z;
extern const char* PARAM_MAX_CLUSTER_AREA;
extern const char* PARAM_TEMPORAL_MODE;
extern const char* PARAM_STUCK_WINDOW_FRAMES;
extern const char* PARAM_STUCK_MIN_FRACTION;
extern const char* PARAM_MAX_GAP_FRAMES;
extern const char* PARAM_REPAIR_ENABLE;
extern const char* PARAM_REPAIR_METHOD;

// View mode enum strings
extern const char* VIEW_MODE_OUTPUT;
extern const char* VIEW_MODE_CANDIDATES_OVERLAY;
extern const char* VIEW_MODE_CONFIRMED_OVERLAY;
extern const char* VIEW_MODE_MASK_ONLY;

// Neighborhood enum strings
extern const char* NEIGHBORHOOD_3X3;
extern const char* NEIGHBORHOOD_5X5;

// Temporal mode enum strings
extern const char* TEMPORAL_MODE_OFF;
extern const char* TEMPORAL_MODE_SEQUENTIAL_ONLY;

// Repair method enum strings
extern const char* REPAIR_METHOD_NEIGHBOR_MEDIAN;
extern const char* REPAIR_METHOD_DIRECTIONAL_MEDIAN;

// Default values
extern const double DEFAULT_LUMA_THRESHOLD;
extern const double DEFAULT_WHITENESS_THRESHOLD;
extern const double DEFAULT_ROBUST_Z_3X3;
extern const double DEFAULT_ROBUST_Z_5X5;
extern const int DEFAULT_MAX_CLUSTER_AREA;
extern const int DEFAULT_STUCK_WINDOW_FRAMES;
extern const double DEFAULT_STUCK_MIN_FRACTION;
extern const int DEFAULT_MAX_GAP_FRAMES;

// Define plugin parameters (composed from individual groups)
void defineParameters(OfxParamSetHandle paramSet);

// Individual parameter group definitions
void defineBasicParameters(OfxParamSetHandle paramSet);
void defineDetectionParameters(OfxParamSetHandle paramSet);
void defineTemporalParameters(OfxParamSetHandle paramSet);
void defineRepairParameters(OfxParamSetHandle paramSet);

// Convert parameter values to core configuration
DeadPixelQC::SpatialDetectorParams getSpatialParams(OfxParamSetHandle paramSet, double time);
DeadPixelQC::TemporalTrackerParams getTemporalParams(OfxParamSetHandle paramSet, double time);
DeadPixelQC::RepairParams getRepairParams(OfxParamSetHandle paramSet, double time);

// Get view mode
int getViewMode(OfxParamSetHandle paramSet, double time);

// Check if effect is enabled
bool isEnabled(OfxParamSetHandle paramSet, double time);

} // namespace DeadPixelQC_OFX