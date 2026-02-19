#include "PluginParams.h"
#include "OfxUtil.h"
#include "DebugLog.h"
#include <cstring>

// Declare global suite pointers (defined in PluginMain.cpp)
extern const OfxPropertySuiteV1* gPropertySuite;
extern const OfxParameterSuiteV1* gParamSuite;

namespace DeadPixelQC_OFX {

// Define temporal parameters
void defineTemporalParameters(OfxParamSetHandle paramSet) {
    DEBUG_LOG("Defining temporal parameters");
    
    OfxPropertySetHandle paramProps = nullptr;
    
    // Create temporal group
    gParamSuite->paramDefine(paramSet, kOfxParamTypeGroup, "Temporal", &paramProps);
    gPropertySuite->propSetString(paramProps, kOfxParamPropHint, 0, "Temporal confirmation parameters");
    gPropertySuite->propSetString(paramProps, kOfxPropLabel, 0, "Temporal");
    
    // Temporal mode
    gParamSuite->paramDefine(paramSet, kOfxParamTypeChoice, PARAM_TEMPORAL_MODE, &paramProps);
    gPropertySuite->propSetString(paramProps, kOfxParamPropChoiceOption, 0, TEMPORAL_MODE_OFF);
    gPropertySuite->propSetString(paramProps, kOfxParamPropChoiceOption, 1, TEMPORAL_MODE_SEQUENTIAL_ONLY);
    gPropertySuite->propSetInt(paramProps, kOfxParamPropDefault, 0, 0);
    gPropertySuite->propSetString(paramProps, kOfxParamPropHint, 0, "Temporal tracking mode");
    gPropertySuite->propSetString(paramProps, kOfxParamPropScriptName, 0, "temporalMode");
    gPropertySuite->propSetString(paramProps, kOfxPropLabel, 0, "Temporal Mode");
    gPropertySuite->propSetString(paramProps, kOfxParamPropParent, 0, "Temporal");
    
    // Stuck window frames
    gParamSuite->paramDefine(paramSet, kOfxParamTypeInteger, PARAM_STUCK_WINDOW_FRAMES, &paramProps);
    gPropertySuite->propSetInt(paramProps, kOfxParamPropDefault, 0, DEFAULT_STUCK_WINDOW_FRAMES);
    gPropertySuite->propSetInt(paramProps, kOfxParamPropMin, 0, 1);
    gPropertySuite->propSetInt(paramProps, kOfxParamPropMax, 0, 1000);
    gPropertySuite->propSetInt(paramProps, kOfxParamPropDisplayMin, 0, 1);
    gPropertySuite->propSetInt(paramProps, kOfxParamPropDisplayMax, 0, 300);
    gPropertySuite->propSetString(paramProps, kOfxParamPropHint, 0, "Number of frames in temporal window");
    gPropertySuite->propSetString(paramProps, kOfxParamPropScriptName, 0, "stuckWindowFrames");
    gPropertySuite->propSetString(paramProps, kOfxPropLabel, 0, "Window Frames");
    gPropertySuite->propSetString(paramProps, kOfxParamPropParent, 0, "Temporal");
    
    // Stuck min fraction
    gParamSuite->paramDefine(paramSet, kOfxParamTypeDouble, PARAM_STUCK_MIN_FRACTION, &paramProps);
    gPropertySuite->propSetDouble(paramProps, kOfxParamPropDefault, 0, DEFAULT_STUCK_MIN_FRACTION);
    gPropertySuite->propSetDouble(paramProps, kOfxParamPropMin, 0, 0.0);
    gPropertySuite->propSetDouble(paramProps, kOfxParamPropMax, 0, 1.0);
    gPropertySuite->propSetDouble(paramProps, kOfxParamPropDisplayMin, 0, 0.5);
    gPropertySuite->propSetDouble(paramProps, kOfxParamPropDisplayMax, 0, 1.0);
    gPropertySuite->propSetString(paramProps, kOfxParamPropHint, 0, "Minimum fraction of frames where pixel must be detected");
    gPropertySuite->propSetString(paramProps, kOfxParamPropScriptName, 0, "stuckMinFraction");
    gPropertySuite->propSetString(paramProps, kOfxPropLabel, 0, "Min Fraction");
    gPropertySuite->propSetString(paramProps, kOfxParamPropParent, 0, "Temporal");
    
    // Max gap frames
    gParamSuite->paramDefine(paramSet, kOfxParamTypeInteger, PARAM_MAX_GAP_FRAMES, &paramProps);
    gPropertySuite->propSetInt(paramProps, kOfxParamPropDefault, 0, DEFAULT_MAX_GAP_FRAMES);
    gPropertySuite->propSetInt(paramProps, kOfxParamPropMin, 0, 0);
    gPropertySuite->propSetInt(paramProps, kOfxParamPropMax, 0, 10);
    gPropertySuite->propSetInt(paramProps, kOfxParamPropDisplayMin, 0, 0);
    gPropertySuite->propSetInt(paramProps, kOfxParamPropDisplayMax, 0, 10);
    gPropertySuite->propSetString(paramProps, kOfxParamPropHint, 0, "Maximum allowed gap between detections");
    gPropertySuite->propSetString(paramProps, kOfxParamPropScriptName, 0, "maxGapFrames");
    gPropertySuite->propSetString(paramProps, kOfxPropLabel, 0, "Max Gap Frames");
    gPropertySuite->propSetString(paramProps, kOfxParamPropParent, 0, "Temporal");
}

} // namespace DeadPixelQC_OFX