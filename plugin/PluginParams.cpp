#include "PluginParams.h"
#include "DebugLog.h"

namespace DeadPixelQC_OFX {

// Main function to define all parameters
void defineParameters(OfxParamSetHandle paramSet) {
    DEBUG_LOG("Defining all plugin parameters");
    
    // Define parameters in logical groups
    defineBasicParameters(paramSet);
    defineDetectionParameters(paramSet);
    defineTemporalParameters(paramSet);
    defineRepairParameters(paramSet);
    defineWorkflowParameters(paramSet);
    
    DEBUG_LOG("All parameters defined successfully");
}

} // namespace DeadPixelQC_OFX