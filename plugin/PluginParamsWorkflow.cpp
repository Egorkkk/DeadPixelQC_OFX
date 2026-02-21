#include "PluginParams.h"
#include "OfxUtil.h"
#include "DebugLog.h"
#include <cstring>

namespace DeadPixelQC_OFX {

// Define workflow mode and status parameters
void defineWorkflowParameters(OfxParamSetHandle paramSet) {
    DEBUG_LOG("Defining workflow parameters");

    if (!paramSet || !gParamSuite || !gPropertySuite) {
        return;
    }

    OfxParamHandle existing = nullptr;
    if (gParamSuite->paramGetHandle(paramSet, PARAM_WORKFLOW_MODE, &existing, nullptr) == kOfxStatOK && existing) {
        return;
    }

    OfxPropertySetHandle paramProps = nullptr;
    auto defineParam = [&](const char* type, const char* name) -> bool {
        return gParamSuite->paramDefine(paramSet, type, name, &paramProps) == kOfxStatOK && paramProps != nullptr;
    };
    auto setString = [&](const char* prop, int index, const char* value) -> bool {
        return gPropertySuite->propSetString(paramProps, prop, index, value) == kOfxStatOK;
    };
    auto setInt = [&](const char* prop, int index, int value) -> bool {
        return gPropertySuite->propSetInt(paramProps, prop, index, value) == kOfxStatOK;
    };
    
    // Create Workflow group
    if (!defineParam(kOfxParamTypeGroup, "Workflow")) return;
    if (!setString(kOfxParamPropHint, 0, "Workflow mode and status")) return;
    if (!setString(kOfxPropLabel, 0, "Workflow")) return;
    
    // Workflow Mode choice parameter
    if (!defineParam(kOfxParamTypeChoice, PARAM_WORKFLOW_MODE)) return;
    if (!setString(kOfxParamPropChoiceOption, 0, WORKFLOW_MODE_QC_SCAN)) return;
    if (!setString(kOfxParamPropChoiceOption, 1, WORKFLOW_MODE_MAP_BUILD)) return;
    if (!setString(kOfxParamPropChoiceOption, 2, WORKFLOW_MODE_MAP_APPLY)) return;
    if (!setInt(kOfxParamPropDefault, 0, DEFAULT_WORKFLOW_MODE)) return;
    if (!setString(kOfxParamPropHint, 0, "Workflow mode: QC_Scan for detection only, Map_Build to accumulate defects, Map_Apply to use loaded map")) return;
    if (!setString(kOfxParamPropScriptName, 0, "workflowMode")) return;
    if (!setString(kOfxPropLabel, 0, "Workflow Mode")) return;
    if (!setString(kOfxParamPropParent, 0, "Workflow")) return;
    
    // Events Count (read-only)
    if (!defineParam(kOfxParamTypeString, PARAM_EVENTS_COUNT)) return;
    if (!setString(kOfxParamPropDefault, 0, DEFAULT_EVENTS_COUNT)) return;
    if (!setString(kOfxParamPropHint, 0, "Total number of detected events")) return;
    if (!setString(kOfxParamPropScriptName, 0, "eventsCount")) return;
    if (!setString(kOfxPropLabel, 0, "Events Count")) return;
    if (!setString(kOfxParamPropParent, 0, "Workflow")) return;
    // Make read-only
    if (!setInt(kOfxParamPropEvaluateOnChange, 0, 0)) return;
    
    // Selected Event Index (read-only)
    if (!defineParam(kOfxParamTypeInteger, PARAM_SELECTED_EVENT_INDEX)) return;
    if (!setInt(kOfxParamPropDefault, 0, DEFAULT_SELECTED_EVENT_INDEX)) return;
    if (!setInt(kOfxParamPropMin, 0, -1)) return;
    if (!setString(kOfxParamPropHint, 0, "Index of currently selected event (-1 = none)")) return;
    if (!setString(kOfxParamPropScriptName, 0, "selectedEventIndex")) return;
    if (!setString(kOfxPropLabel, 0, "Selected Event Index")) return;
    if (!setString(kOfxParamPropParent, 0, "Workflow")) return;
    // Make read-only
    if (!setInt(kOfxParamPropEvaluateOnChange, 0, 0)) return;
    
    // Selected Event Info (read-only)
    if (!defineParam(kOfxParamTypeString, PARAM_SELECTED_EVENT_INFO)) return;
    if (!setString(kOfxParamPropDefault, 0, DEFAULT_SELECTED_EVENT_INFO)) return;
    if (!setString(kOfxParamPropHint, 0, "Information about the selected event")) return;
    if (!setString(kOfxParamPropScriptName, 0, "selectedEventInfo")) return;
    if (!setString(kOfxPropLabel, 0, "Selected Event Info")) return;
    if (!setString(kOfxParamPropParent, 0, "Workflow")) return;
    // Make read-only
    if (!setInt(kOfxParamPropEvaluateOnChange, 0, 0)) return;
    
    // Map Size (read-only)
    if (!defineParam(kOfxParamTypeString, PARAM_MAP_SIZE)) return;
    if (!setString(kOfxParamPropDefault, 0, DEFAULT_MAP_SIZE)) return;
    if (!setString(kOfxParamPropHint, 0, "Number of defect points in current map")) return;
    if (!setString(kOfxParamPropScriptName, 0, "mapSize")) return;
    if (!setString(kOfxPropLabel, 0, "Map Size")) return;
    if (!setString(kOfxParamPropParent, 0, "Workflow")) return;
    // Make read-only
    if (!setInt(kOfxParamPropEvaluateOnChange, 0, 0)) return;
    
    // Map Status (read-only)
    if (!defineParam(kOfxParamTypeString, PARAM_MAP_STATUS)) return;
    if (!setString(kOfxParamPropDefault, 0, DEFAULT_MAP_STATUS)) return;
    if (!setString(kOfxParamPropHint, 0, "Status of the defect map")) return;
    if (!setString(kOfxParamPropScriptName, 0, "mapStatus")) return;
    if (!setString(kOfxPropLabel, 0, "Map Status")) return;
    if (!setString(kOfxParamPropParent, 0, "Workflow")) return;
    // Make read-only
    if (!setInt(kOfxParamPropEvaluateOnChange, 0, 0)) return;

    // Debug Stats (read-only)
    if (!defineParam(kOfxParamTypeString, PARAM_DEBUG_STATS)) return;
    if (!setString(kOfxParamPropDefault, 0, DEFAULT_DEBUG_STATS)) return;
    if (!setString(kOfxParamPropHint, 0, "Internal debug statistics")) return;
    if (!setString(kOfxParamPropScriptName, 0, "debugStats")) return;
    if (!setString(kOfxPropLabel, 0, "Debug Stats")) return;
    if (!setString(kOfxParamPropParent, 0, "Workflow")) return;
    // Make read-only
    if (!setInt(kOfxParamPropEvaluateOnChange, 0, 0)) return;
    

    // PrevEvent button
    if (!defineParam(kOfxParamTypePushButton, PARAM_PREV_EVENT)) return;
    if (!setString(kOfxParamPropHint, 0, "Select previous event")) return;
    if (!setString(kOfxParamPropScriptName, 0, "prevEvent")) return;
    if (!setString(kOfxPropLabel, 0, "< Prev Event")) return;
    if (!setString(kOfxParamPropParent, 0, "Workflow")) return;
    
    // NextEvent button
    if (!defineParam(kOfxParamTypePushButton, PARAM_NEXT_EVENT)) return;
    if (!setString(kOfxParamPropHint, 0, "Select next event")) return;
    if (!setString(kOfxParamPropScriptName, 0, "nextEvent")) return;
    if (!setString(kOfxPropLabel, 0, "Next Event >")) return;
    if (!setString(kOfxParamPropParent, 0, "Workflow")) return;
    
    // ClearEvents button
    if (!defineParam(kOfxParamTypePushButton, PARAM_CLEAR_EVENTS)) return;
    if (!setString(kOfxParamPropHint, 0, "Clear all detected events")) return;
    if (!setString(kOfxParamPropScriptName, 0, "clearEvents")) return;
    if (!setString(kOfxPropLabel, 0, "Clear Events")) return;
    if (!setString(kOfxParamPropParent, 0, "Workflow")) return;
    
    // ExportEvents button
    if (!defineParam(kOfxParamTypePushButton, PARAM_EXPORT_EVENTS)) return;
    if (!setString(kOfxParamPropHint, 0, "Export events to CSV/JSON")) return;
    if (!setString(kOfxParamPropScriptName, 0, "exportEvents")) return;
    if (!setString(kOfxPropLabel, 0, "Export Events")) return;
    if (!setString(kOfxParamPropParent, 0, "Workflow")) return;

    // ExportPath parameter
    if (!defineParam(kOfxParamTypeString, PARAM_EXPORT_PATH)) return;
    if (!setString(kOfxParamPropDefault, 0, DEFAULT_EXPORT_PATH)) return;
    if (!setString(kOfxParamPropHint, 0, "Path to export events file")) return;
    if (!setString(kOfxParamPropScriptName, 0, "exportPath")) return;
    if (!setString(kOfxPropLabel, 0, "Export Path")) return;
    if (!setString(kOfxParamPropParent, 0, "Workflow")) return;

    // ExportFormat parameter
    if (!defineParam(kOfxParamTypeChoice, PARAM_EXPORT_FORMAT)) return;
    if (!setString(kOfxParamPropChoiceOption, 0, EXPORT_FORMAT_CSV)) return;
    if (!setString(kOfxParamPropChoiceOption, 1, EXPORT_FORMAT_JSON)) return;
    if (!setInt(kOfxParamPropDefault, 0, DEFAULT_EXPORT_FORMAT)) return;
    if (!setString(kOfxParamPropHint, 0, "Export file format")) return;
    if (!setString(kOfxParamPropScriptName, 0, "exportFormat")) return;
    if (!setString(kOfxPropLabel, 0, "Export Format")) return;
    if (!setString(kOfxParamPropParent, 0, "Workflow")) return;

    // ExportStatus parameter (read-only)
    if (!defineParam(kOfxParamTypeString, PARAM_EXPORT_STATUS)) return;
    if (!setString(kOfxParamPropDefault, 0, DEFAULT_EXPORT_STATUS)) return;
    if (!setString(kOfxParamPropHint, 0, "Status of the latest export")) return;
    if (!setString(kOfxParamPropScriptName, 0, "exportStatus")) return;
    if (!setString(kOfxPropLabel, 0, "Export Status")) return;
    if (!setString(kOfxParamPropParent, 0, "Workflow")) return;
    if (!setInt(kOfxParamPropEvaluateOnChange, 0, 0)) return;

    // MapPath parameter
    if (!defineParam(kOfxParamTypeString, PARAM_MAP_PATH)) return;
    if (!setString(kOfxParamPropDefault, 0, DEFAULT_MAP_PATH)) return;
    if (!setString(kOfxParamPropHint, 0, "Path to load/save defect map JSON")) return;
    if (!setString(kOfxParamPropScriptName, 0, "mapPath")) return;
    if (!setString(kOfxPropLabel, 0, "Map Path")) return;
    if (!setString(kOfxParamPropParent, 0, "Workflow")) return;

    // CameraTag parameter
    if (!defineParam(kOfxParamTypeString, PARAM_CAMERA_TAG)) return;
    if (!setString(kOfxParamPropDefault, 0, DEFAULT_CAMERA_TAG)) return;
    if (!setString(kOfxParamPropHint, 0, "Camera tag saved into map metadata")) return;
    if (!setString(kOfxParamPropScriptName, 0, "cameraTag")) return;
    if (!setString(kOfxPropLabel, 0, "Camera Tag")) return;
    if (!setString(kOfxParamPropParent, 0, "Workflow")) return;
    
    // LoadMap button
    if (!defineParam(kOfxParamTypePushButton, PARAM_LOAD_MAP)) return;
    if (!setString(kOfxParamPropHint, 0, "Load defect map from file")) return;
    if (!setString(kOfxParamPropScriptName, 0, "loadMap")) return;
    if (!setString(kOfxPropLabel, 0, "Load Map")) return;
    if (!setString(kOfxParamPropParent, 0, "Workflow")) return;
    
    // SaveMap button
    if (!defineParam(kOfxParamTypePushButton, PARAM_SAVE_MAP)) return;
    if (!setString(kOfxParamPropHint, 0, "Save current defect map to file")) return;
    if (!setString(kOfxParamPropScriptName, 0, "saveMap")) return;
    if (!setString(kOfxPropLabel, 0, "Save Map")) return;
    if (!setString(kOfxParamPropParent, 0, "Workflow")) return;
    
    // ResetMap button
    if (!defineParam(kOfxParamTypePushButton, PARAM_RESET_MAP)) return;
    if (!setString(kOfxParamPropHint, 0, "Reset / clear the defect map")) return;
    if (!setString(kOfxParamPropScriptName, 0, "resetMap")) return;
    if (!setString(kOfxPropLabel, 0, "Reset Map")) return;
    if (!setString(kOfxParamPropParent, 0, "Workflow")) return;
    
    // FinalizeMap button
    if (!defineParam(kOfxParamTypePushButton, PARAM_FINALIZE_MAP)) return;
    if (!setString(kOfxParamPropHint, 0, "Finalize and commit the current map")) return;
    if (!setString(kOfxParamPropScriptName, 0, "finalizeMap")) return;
    if (!setString(kOfxPropLabel, 0, "Finalize Map")) return;
    if (!setString(kOfxParamPropParent, 0, "Workflow")) return;
}

} // namespace DeadPixelQC_OFX
