#include "PluginParams.h"
#include "OfxUtil.h"
#include "DebugLog.h"
#include <cstring>

// Declare global suite pointers (defined in PluginMain.cpp)
extern const OfxPropertySuiteV1* gPropertySuite;
extern const OfxParameterSuiteV1* gParamSuite;

namespace DeadPixelQC_OFX {

// Define basic parameters
void defineBasicParameters(OfxParamSetHandle paramSet) {
    DEBUG_LOG("Defining basic parameters");
    
    OfxPropertySetHandle paramProps = nullptr;
    
    // Enable parameter
    gParamSuite->paramDefine(paramSet, kOfxParamTypeBoolean, PARAM_ENABLE, &paramProps);
    gPropertySuite->propSetInt(paramProps, kOfxParamPropDefault, 0, 1);
    gPropertySuite->propSetString(paramProps, kOfxParamPropHint, 0, "Enable/disable the effect");
    gPropertySuite->propSetString(paramProps, kOfxParamPropScriptName, 0, "enable");
    gPropertySuite->propSetString(paramProps, kOfxPropLabel, 0, "Enable");
    
    // View mode parameter
    gParamSuite->paramDefine(paramSet, kOfxParamTypeChoice, PARAM_VIEW_MODE, &paramProps);
    gPropertySuite->propSetString(paramProps, kOfxParamPropChoiceOption, 0, VIEW_MODE_OUTPUT);
    gPropertySuite->propSetString(paramProps, kOfxParamPropChoiceOption, 1, VIEW_MODE_CANDIDATES_OVERLAY);
    gPropertySuite->propSetString(paramProps, kOfxParamPropChoiceOption, 2, VIEW_MODE_CONFIRMED_OVERLAY);
    gPropertySuite->propSetString(paramProps, kOfxParamPropChoiceOption, 3, VIEW_MODE_MASK_ONLY);
    gPropertySuite->propSetInt(paramProps, kOfxParamPropDefault, 0, 0);
    gPropertySuite->propSetString(paramProps, kOfxParamPropHint, 0, "Output view mode");
    gPropertySuite->propSetString(paramProps, kOfxParamPropScriptName, 0, "viewMode");
    gPropertySuite->propSetString(paramProps, kOfxPropLabel, 0, "View Mode");
}

} // namespace DeadPixelQC_OFX