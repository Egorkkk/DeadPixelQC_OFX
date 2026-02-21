#include "PluginParams.h"
#include "OfxUtil.h"
#include "DebugLog.h"
#include <cstring>

namespace DeadPixelQC_OFX {

// Define repair parameters
void defineRepairParameters(OfxParamSetHandle paramSet) {
    DEBUG_LOG("Defining repair parameters");
    
    OfxPropertySetHandle paramProps = nullptr;
    
    // Create repair group
    gParamSuite->paramDefine(paramSet, kOfxParamTypeGroup, "Repair", &paramProps);
    gPropertySuite->propSetString(paramProps, kOfxParamPropHint, 0, "Pixel repair parameters");
    gPropertySuite->propSetString(paramProps, kOfxPropLabel, 0, "Repair");
    
    // Repair enable
    gParamSuite->paramDefine(paramSet, kOfxParamTypeBoolean, PARAM_REPAIR_ENABLE, &paramProps);
    gPropertySuite->propSetInt(paramProps, kOfxParamPropDefault, 0, 0);
    gPropertySuite->propSetString(paramProps, kOfxParamPropHint, 0, "Enable pixel repair (Phase 3)");
    gPropertySuite->propSetString(paramProps, kOfxParamPropScriptName, 0, "repairEnable");
    gPropertySuite->propSetString(paramProps, kOfxPropLabel, 0, "Enable Repair");
    gPropertySuite->propSetString(paramProps, kOfxParamPropParent, 0, "Repair");
    
    // Repair method
    gParamSuite->paramDefine(paramSet, kOfxParamTypeChoice, PARAM_REPAIR_METHOD, &paramProps);
    gPropertySuite->propSetString(paramProps, kOfxParamPropChoiceOption, 0, REPAIR_METHOD_NEIGHBOR_MEDIAN);
    gPropertySuite->propSetString(paramProps, kOfxParamPropChoiceOption, 1, REPAIR_METHOD_DIRECTIONAL_MEDIAN);
    gPropertySuite->propSetInt(paramProps, kOfxParamPropDefault, 0, 0);
    gPropertySuite->propSetString(paramProps, kOfxParamPropHint, 0, "Pixel repair method");
    gPropertySuite->propSetString(paramProps, kOfxParamPropScriptName, 0, "repairMethod");
    gPropertySuite->propSetString(paramProps, kOfxPropLabel, 0, "Repair Method");
    gPropertySuite->propSetString(paramProps, kOfxParamPropParent, 0, "Repair");
}

} // namespace DeadPixelQC_OFX
