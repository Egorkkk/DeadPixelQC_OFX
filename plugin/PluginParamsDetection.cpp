#include "PluginParams.h"
#include "OfxUtil.h"
#include "DebugLog.h"
#include <cstring>

namespace DeadPixelQC_OFX {

// Define detection parameters
void defineDetectionParameters(OfxParamSetHandle paramSet) {
    DEBUG_LOG("Defining detection parameters");
    
    OfxPropertySetHandle paramProps = nullptr;
    
    // Create detection group
    gParamSuite->paramDefine(paramSet, kOfxParamTypeGroup, "Detection", &paramProps);
    gPropertySuite->propSetString(paramProps, kOfxParamPropHint, 0, "Detection parameters");
    gPropertySuite->propSetString(paramProps, kOfxPropLabel, 0, "Detection");
    
    // Luma threshold
    gParamSuite->paramDefine(paramSet, kOfxParamTypeDouble, PARAM_LUMA_THRESHOLD, &paramProps);
    gPropertySuite->propSetDouble(paramProps, kOfxParamPropDefault, 0, DEFAULT_LUMA_THRESHOLD);
    gPropertySuite->propSetDouble(paramProps, kOfxParamPropMin, 0, 0.0);
    gPropertySuite->propSetDouble(paramProps, kOfxParamPropMax, 0, 1.0);
    gPropertySuite->propSetDouble(paramProps, kOfxParamPropDisplayMin, 0, 0.9);
    gPropertySuite->propSetDouble(paramProps, kOfxParamPropDisplayMax, 0, 1.0);
    gPropertySuite->propSetString(paramProps, kOfxParamPropHint, 0, "Minimum luma (brightness) for candidate pixels");
    gPropertySuite->propSetString(paramProps, kOfxParamPropScriptName, 0, "lumaThreshold");
    gPropertySuite->propSetString(paramProps, kOfxPropLabel, 0, "Luma Threshold");
    gPropertySuite->propSetString(paramProps, kOfxParamPropParent, 0, "Detection");
    
    // Whiteness threshold
    gParamSuite->paramDefine(paramSet, kOfxParamTypeDouble, PARAM_WHITENESS_THRESHOLD, &paramProps);
    gPropertySuite->propSetDouble(paramProps, kOfxParamPropDefault, 0, DEFAULT_WHITENESS_THRESHOLD);
    gPropertySuite->propSetDouble(paramProps, kOfxParamPropMin, 0, 0.0);
    gPropertySuite->propSetDouble(paramProps, kOfxParamPropMax, 0, 3.0);
    gPropertySuite->propSetDouble(paramProps, kOfxParamPropDisplayMin, 0, 0.0);
    gPropertySuite->propSetDouble(paramProps, kOfxParamPropDisplayMax, 0, 0.2);
    gPropertySuite->propSetString(paramProps, kOfxParamPropHint, 0, "Maximum whiteness (sum of channel differences)");
    gPropertySuite->propSetString(paramProps, kOfxParamPropScriptName, 0, "whitenessThreshold");
    gPropertySuite->propSetString(paramProps, kOfxPropLabel, 0, "Whiteness Threshold");
    gPropertySuite->propSetString(paramProps, kOfxParamPropParent, 0, "Detection");
    
    // Neighborhood
    gParamSuite->paramDefine(paramSet, kOfxParamTypeChoice, PARAM_NEIGHBORHOOD, &paramProps);
    gPropertySuite->propSetString(paramProps, kOfxParamPropChoiceOption, 0, NEIGHBORHOOD_3X3);
    gPropertySuite->propSetString(paramProps, kOfxParamPropChoiceOption, 1, NEIGHBORHOOD_5X5);
    gPropertySuite->propSetInt(paramProps, kOfxParamPropDefault, 0, 0);
    gPropertySuite->propSetString(paramProps, kOfxParamPropHint, 0, "Neighborhood size for robust contrast detection");
    gPropertySuite->propSetString(paramProps, kOfxParamPropScriptName, 0, "neighborhood");
    gPropertySuite->propSetString(paramProps, kOfxPropLabel, 0, "Neighborhood");
    gPropertySuite->propSetString(paramProps, kOfxParamPropParent, 0, "Detection");
    
    // Robust Z-score threshold
    gParamSuite->paramDefine(paramSet, kOfxParamTypeDouble, PARAM_ROBUST_Z, &paramProps);
    gPropertySuite->propSetDouble(paramProps, kOfxParamPropDefault, 0, DEFAULT_ROBUST_Z_3X3);
    gPropertySuite->propSetDouble(paramProps, kOfxParamPropMin, 0, 0.0);
    gPropertySuite->propSetDouble(paramProps, kOfxParamPropMax, 0, 50.0);
    gPropertySuite->propSetDouble(paramProps, kOfxParamPropDisplayMin, 0, 0.0);
    gPropertySuite->propSetDouble(paramProps, kOfxParamPropDisplayMax, 0, 20.0);
    gPropertySuite->propSetString(paramProps, kOfxParamPropHint, 0, "Robust Z-score threshold (higher = more strict)");
    gPropertySuite->propSetString(paramProps, kOfxParamPropScriptName, 0, "robustZ");
    gPropertySuite->propSetString(paramProps, kOfxPropLabel, 0, "Robust Z-Score");
    gPropertySuite->propSetString(paramProps, kOfxParamPropParent, 0, "Detection");
    
    // Max cluster area
    gParamSuite->paramDefine(paramSet, kOfxParamTypeInteger, PARAM_MAX_CLUSTER_AREA, &paramProps);
    gPropertySuite->propSetInt(paramProps, kOfxParamPropDefault, 0, DEFAULT_MAX_CLUSTER_AREA);
    gPropertySuite->propSetInt(paramProps, kOfxParamPropMin, 0, 1);
    gPropertySuite->propSetInt(paramProps, kOfxParamPropMax, 0, 16);
    gPropertySuite->propSetInt(paramProps, kOfxParamPropDisplayMin, 0, 1);
    gPropertySuite->propSetInt(paramProps, kOfxParamPropDisplayMax, 0, 16);
    gPropertySuite->propSetString(paramProps, kOfxParamPropHint, 0, "Maximum area of pixel clusters to consider");
    gPropertySuite->propSetString(paramProps, kOfxParamPropScriptName, 0, "maxClusterArea");
    gPropertySuite->propSetString(paramProps, kOfxPropLabel, 0, "Max Cluster Area");
    gPropertySuite->propSetString(paramProps, kOfxParamPropParent, 0, "Detection");
}

} // namespace DeadPixelQC_OFX
