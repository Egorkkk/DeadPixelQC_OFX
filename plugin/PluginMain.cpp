// DeadPixelQC OFX Plugin
// Copyright (c) 2025

#include "ofxCore.h"
#include "ofxImageEffect.h"
#include "ofxMemory.h"
#include "ofxMultiThread.h"
#include "ofxMessage.h"
#include "ofxParam.h"

#include "PluginParams.h"
#include "OfxUtil.h"
#include "DebugLog.h"

#include "../core/SpatialDetector.h"
#include "../core/TemporalTracker.h"
#include "../core/Fixer.h"
#include "../core/PixelFormatAdapter.h"

#include <memory>
#include <mutex>
#include <vector>
#include <string>
#include <cstring>

using namespace DeadPixelQC;

// Global OFX host pointer
static const OfxHost* gHost = nullptr;

// Global OFX suite pointers
static const OfxImageEffectSuiteV1* gEffectSuite = nullptr;
static const OfxPropertySuiteV1* gPropertySuite = nullptr;
static const OfxParameterSuiteV1* gParamSuite = nullptr;
static const OfxMemorySuiteV1* gMemorySuite = nullptr;
static const OfxMultiThreadSuiteV1* gMultiThreadSuite = nullptr;
static const OfxMessageSuiteV1* gMessageSuite = nullptr;

// Plugin instance data
struct PluginInstanceData {
    // Core processing objects
    std::unique_ptr<DeadPixelQC::SpatialDetector> spatialDetector;
    std::unique_ptr<DeadPixelQC::TemporalTracker> temporalTracker;
    std::unique_ptr<DeadPixelQC::Fixer> fixer;
    
    // Clip handles
    OfxImageClipHandle sourceClip = nullptr;
    OfxImageClipHandle outputClip = nullptr;
    
    // Current frame index (for temporal tracking)
    i32 currentFrameIndex = -1;
    
    // Mutex for thread safety
    std::mutex mutex;
    
    // Reset temporal state (e.g., when scrubbing)
    void resetTemporalState() {
        if (temporalTracker) {
            temporalTracker->resetTemporalState();
        }
        currentFrameIndex = -1;
    }
};

// Get instance data from effect handle
static PluginInstanceData* getInstanceData(OfxImageEffectHandle effect) {
    OfxPropertySetHandle effectProps = nullptr;
    gEffectSuite->getPropertySet(effect, &effectProps);
    
    PluginInstanceData* instanceData = nullptr;
    gPropertySuite->propGetPointer(effectProps, kOfxPropInstanceData, 0, 
                                   reinterpret_cast<void**>(&instanceData));
    return instanceData;
}

// Set instance data to effect handle
static void setInstanceData(OfxImageEffectHandle effect, PluginInstanceData* instanceData) {
    OfxPropertySetHandle effectProps = nullptr;
    gEffectSuite->getPropertySet(effect, &effectProps);
    
    gPropertySuite->propSetPointer(effectProps, kOfxPropInstanceData, 0, instanceData);
}

// Fetch host suites using the host's fetchSuite function
static OfxStatus fetchHostSuites() {
    DEBUG_LOG("Fetching host suites");
    
    if (!gHost || !gHost->fetchSuite) {
        DEBUG_LOG_ERROR("Host or fetchSuite is null");
        return kOfxStatErrMissingHostFeature;
    }
    
    // Fetch suites using the host's fetchSuite function
    gEffectSuite = reinterpret_cast<const OfxImageEffectSuiteV1*>(
        gHost->fetchSuite(gHost->host, kOfxImageEffectSuite, 1));
    
    gPropertySuite = reinterpret_cast<const OfxPropertySuiteV1*>(
        gHost->fetchSuite(gHost->host, kOfxPropertySuite, 1));
    
    gParamSuite = reinterpret_cast<const OfxParameterSuiteV1*>(
        gHost->fetchSuite(gHost->host, kOfxParameterSuite, 1));
    
    gMemorySuite = reinterpret_cast<const OfxMemorySuiteV1*>(
        gHost->fetchSuite(gHost->host, kOfxMemorySuite, 1));
    
    gMultiThreadSuite = reinterpret_cast<const OfxMultiThreadSuiteV1*>(
        gHost->fetchSuite(gHost->host, kOfxMultiThreadSuite, 1));
    
    gMessageSuite = reinterpret_cast<const OfxMessageSuiteV1*>(
        gHost->fetchSuite(gHost->host, kOfxMessageSuite, 1));
    
    if (!gEffectSuite || !gPropertySuite || !gParamSuite || !gMemorySuite || 
        !gMultiThreadSuite || !gMessageSuite) {
        DEBUG_LOG_ERROR("Failed to fetch one or more suites");
        return kOfxStatErrMissingHostFeature;
    }
    
    DEBUG_LOG("All suites fetched successfully");
    return kOfxStatOK;
}

// Called when plugin is loaded
static OfxStatus onLoad() {
    DEBUG_LOG_ACTION(kOfxActionLoad, "");
    return fetchHostSuites();
}

// Called before plugin is unloaded
static OfxStatus onUnload() {
    DEBUG_LOG_ACTION(kOfxActionUnload, "");
    return kOfxStatOK;
}

// Create plugin instance
static OfxStatus createInstance(OfxImageEffectHandle effect) {
    DEBUG_LOG_ACTION(kOfxActionCreateInstance, "Creating instance");
    
    try {
        // Create instance data
        auto instanceData = std::make_unique<PluginInstanceData>();
        
        // Get clip handles
        gEffectSuite->clipGetHandle(effect, kOfxImageEffectSimpleSourceClipName, 
                                    &instanceData->sourceClip, nullptr);
        gEffectSuite->clipGetHandle(effect, kOfxImageEffectOutputClipName, 
                                    &instanceData->outputClip, nullptr);
        
        // Create core objects (will be configured with parameters later)
        instanceData->spatialDetector = std::make_unique<DeadPixelQC::SpatialDetector>();
        instanceData->temporalTracker = std::make_unique<DeadPixelQC::TemporalTracker>();
        instanceData->fixer = std::make_unique<DeadPixelQC::Fixer>();
        
        // Store instance data
        setInstanceData(effect, instanceData.release());
        
        DEBUG_LOG("Instance created successfully");
        return kOfxStatOK;
    } catch (const std::exception& e) {
        DEBUG_LOG_ERROR(std::string("Failed to create instance: ") + e.what());
        DeadPixelQC_OFX::OfxUtil::reportError(effect, std::string("Failed to create instance: ") + e.what());
        return kOfxStatErrMemory;
    }
}

// Destroy plugin instance
static OfxStatus destroyInstance(OfxImageEffectHandle effect) {
    DEBUG_LOG_ACTION(kOfxActionDestroyInstance, "Destroying instance");
    
    PluginInstanceData* instanceData = getInstanceData(effect);
    if (instanceData) {
        delete instanceData;
        setInstanceData(effect, nullptr);
    }
    return kOfxStatOK;
}

// Describe plugin capabilities
static OfxStatus describe(OfxImageEffectHandle effect) {
    DEBUG_LOG_ACTION(kOfxActionDescribe, "Describing plugin");
    
    OfxPropertySetHandle effectProps = nullptr;
    gEffectSuite->getPropertySet(effect, &effectProps);
    
    // Set plugin properties
    gPropertySuite->propSetString(effectProps, kOfxPropLabel, 0, "DeadPixelQC");
    gPropertySuite->propSetString(effectProps, kOfxImageEffectPluginPropGrouping, 0, "Quality Control");
    
    // Support multiple contexts (filter and general)
    gPropertySuite->propSetString(effectProps, kOfxImageEffectPropSupportedContexts, 0, 
                                  kOfxImageEffectContextFilter);
    gPropertySuite->propSetString(effectProps, kOfxImageEffectPropSupportedContexts, 1, 
                                  kOfxImageEffectContextGeneral);
    
    // Support multiple pixel depths
    gPropertySuite->propSetString(effectProps, kOfxImageEffectPropSupportedPixelDepths, 0, 
                                  kOfxBitDepthByte);
    gPropertySuite->propSetString(effectProps, kOfxImageEffectPropSupportedPixelDepths, 1, 
                                  kOfxBitDepthShort);
    gPropertySuite->propSetString(effectProps, kOfxImageEffectPropSupportedPixelDepths, 2, 
                                  kOfxBitDepthFloat);
    
    // Support RGBA and RGB components
    gPropertySuite->propSetString(effectProps, kOfxImageEffectPropSupportedComponents, 0, 
                                  kOfxImageComponentRGBA);
    gPropertySuite->propSetString(effectProps, kOfxImageEffectPropSupportedComponents, 1, 
                                  kOfxImageComponentRGB);
    
    // Enable host frame threading
    gPropertySuite->propSetInt(effectProps, kOfxImageEffectPluginPropHostFrameThreading, 0, 1);
    
    // Render thread safety
    gPropertySuite->propSetString(effectProps, kOfxImageEffectPluginRenderThreadSafety, 0, 
                                  kOfxImageEffectRenderInstanceSafe);
    
    DEBUG_LOG("Plugin description completed");
    return kOfxStatOK;
}

// Describe plugin in specific context
static OfxStatus describeInContext(OfxImageEffectHandle effect, OfxPropertySetHandle inArgs) {
    DEBUG_LOG_ACTION(kOfxImageEffectActionDescribeInContext, "Describing in context");
    
    // Get context
    char* context = nullptr;
    gPropertySuite->propGetString(inArgs, kOfxImageEffectPropContext, 0, &context);
    
    // Define clips
    OfxPropertySetHandle clipProps = nullptr;
    
    // Output clip
    gEffectSuite->clipDefine(effect, kOfxImageEffectOutputClipName, &clipProps);
    gPropertySuite->propSetString(clipProps, kOfxImageEffectPropSupportedComponents, 0, 
                                  kOfxImageComponentRGBA);
    gPropertySuite->propSetString(clipProps, kOfxImageEffectPropSupportedComponents, 1, 
                                  kOfxImageComponentRGB);
    
    // Source clip
    gEffectSuite->clipDefine(effect, kOfxImageEffectSimpleSourceClipName, &clipProps);
    gPropertySuite->propSetString(clipProps, kOfxImageEffectPropSupportedComponents, 0, 
                                  kOfxImageComponentRGBA);
    gPropertySuite->propSetString(clipProps, kOfxImageEffectPropSupportedComponents, 1, 
                                  kOfxImageComponentRGB);
    
    // Get parameter set
    OfxParamSetHandle paramSet = nullptr;
    gEffectSuite->getParamSet(effect, &paramSet);
    
    // Define parameters
    DeadPixelQC_OFX::defineParameters(paramSet);
    
    DEBUG_LOG("Context description completed");
    return kOfxStatOK;
}

// Get clip preferences
static OfxStatus getClipPreferences(OfxImageEffectHandle effect, 
                                    OfxPropertySetHandle /*inArgs*/, 
                                    OfxPropertySetHandle outArgs) {
    DEBUG_LOG_ACTION(kOfxImageEffectActionGetClipPreferences, "Getting clip preferences");
    
    // Set output to match input format
    gPropertySuite->propSetString(outArgs, "OfxImageClipPropComponents_Output", 0, 
                                  kOfxImageComponentRGBA);
    
    return kOfxStatOK;
}

// Instance changed callback
static OfxStatus instanceChanged(OfxImageEffectHandle effect,
                                 OfxPropertySetHandle inArgs,
                                 OfxPropertySetHandle /*outArgs*/) {
    DEBUG_LOG_ACTION(kOfxActionInstanceChanged, "Instance changed");
    
    // Check if temporal state needs to be reset
    char* changeReason = nullptr;
    gPropertySuite->propGetString(inArgs, kOfxPropChangeReason, 0, &changeReason);
    
    if (changeReason && strcmp(changeReason, kOfxChangeTime) == 0) {
        // Time changed - check if we're scrubbing
        PluginInstanceData* instanceData = getInstanceData(effect);
        if (instanceData) {
            // In a real implementation, we'd check if the time jump is large
            // For now, we'll reset on any time change for safety
            instanceData->resetTemporalState();
        }
    }
    
    return kOfxStatOK;
}

// Begin sequence render
static OfxStatus beginSequenceRender(OfxImageEffectHandle effect,
                                     OfxPropertySetHandle inArgs,
                                     OfxPropertySetHandle /*outArgs*/) {
    DEBUG_LOG_ACTION(kOfxImageEffectActionBeginSequenceRender, "Begin sequence render");
    
    PluginInstanceData* instanceData = getInstanceData(effect);
    if (!instanceData) {
        return kOfxStatErrBadHandle;
    }
    
    // Reset temporal state at start of sequence
    instanceData->resetTemporalState();
    
    return kOfxStatOK;
}

// End sequence render
static OfxStatus endSequenceRender(OfxImageEffectHandle effect,
                                   OfxPropertySetHandle /*inArgs*/,
                                   OfxPropertySetHandle /*outArgs*/) {
    DEBUG_LOG_ACTION(kOfxImageEffectActionEndSequenceRender, "End sequence render");
    return kOfxStatOK;
}

// Render a frame
static OfxStatus render(OfxImageEffectHandle effect,
                        OfxPropertySetHandle inArgs,
                        OfxPropertySetHandle /*outArgs*/) {
    DEBUG_LOG_ACTION(kOfxImageEffectActionRender, "Rendering frame");
    
    PluginInstanceData* instanceData = getInstanceData(effect);
    if (!instanceData || !instanceData->spatialDetector || 
        !instanceData->temporalTracker || !instanceData->fixer) {
        return kOfxStatErrBadHandle;
    }
    
    // Get render time
    double time = 0.0;
    gPropertySuite->propGetDouble(inArgs, kOfxPropTime, 0, &time);
    
    // Get parameter set
    OfxParamSetHandle paramSet = nullptr;
    gEffectSuite->getParamSet(effect, &paramSet);
    
    // Check if effect is enabled
    if (!DeadPixelQC_OFX::isEnabled(paramSet, time)) {
        // Effect disabled - pass through source
        OfxPropertySetHandle sourceImg = nullptr;
        OfxPropertySetHandle outputImg = nullptr;
        
        // Get source image
        gEffectSuite->clipGetImage(instanceData->sourceClip, time, nullptr, &sourceImg);
        if (!sourceImg) {
            return kOfxStatFailed;
        }
        
        // Get output image
        gEffectSuite->clipGetImage(instanceData->outputClip, time, nullptr, &outputImg);
        if (!outputImg) {
            gEffectSuite->clipReleaseImage(sourceImg);
            return kOfxStatFailed;
        }
        
        // Copy source to output (simple pass-through)
        // In a real implementation, we'd copy pixel data
        // For now, we'll just release the images
        
        gEffectSuite->clipReleaseImage(sourceImg);
        gEffectSuite->clipReleaseImage(outputImg);
        return kOfxStatOK;
    }
    
    // Get parameters
    auto spatialParams = DeadPixelQC_OFX::getSpatialParams(paramSet, time);
    auto temporalParams = DeadPixelQC_OFX::getTemporalParams(paramSet, time);
    auto repairParams = DeadPixelQC_OFX::getRepairParams(paramSet, time);
    int viewMode = DeadPixelQC_OFX::getViewMode(paramSet, time);
    
    // Update core objects with parameters
    instanceData->spatialDetector->setParams(spatialParams);
    instanceData->temporalTracker->setParams(temporalParams);
    instanceData->fixer->setParams(repairParams);
    
    // Get source and output images
    OfxPropertySetHandle sourceImg = nullptr;
    OfxPropertySetHandle outputImg = nullptr;
    
    OfxStatus status = gEffectSuite->clipGetImage(instanceData->sourceClip, time, nullptr, &sourceImg);
    if (status != kOfxStatOK || !sourceImg) {
        return kOfxStatFailed;
    }
    
    status = gEffectSuite->clipGetImage(instanceData->outputClip, time, nullptr, &outputImg);
    if (status != kOfxStatOK || !outputImg) {
        gEffectSuite->clipReleaseImage(sourceImg);
        return kOfxStatFailed;
    }
    
    try {
        // Convert OFX images to our buffer format
        auto sourceBuffer = DeadPixelQC_OFX::OfxUtil::getImageBuffer(sourceImg);
        auto outputBuffer = DeadPixelQC_OFX::OfxUtil::getImageBuffer(outputImg);
        
        // Process frame
        auto spatialResult = instanceData->spatialDetector->processFrame(sourceBuffer, 
                                                                         static_cast<i32>(time));
        
        // Apply temporal tracking
        auto temporalResult = instanceData->temporalTracker->processFrame(spatialResult);
        
        // Copy source to output
        // In a real implementation, we'd copy pixel data based on view mode
        // For now, we'll implement a simple copy
        
        // TODO: Implement proper rendering based on view mode:
        // - Output: pass-through or repaired
        // - Overlay modes: blend markers
        // - Mask-only: output mask
        
        // Simple copy for now
        const i32 width = sourceBuffer.width();
        const i32 height = sourceBuffer.height();
        
        for (i32 y = 0; y < height; ++y) {
            for (i32 x = 0; x < width; ++x) {
                f32 r, g, b;
                sourceBuffer.getRGBNormalized(x, y, r, g, b);
                outputBuffer.setPixelNormalized(x, y, r, g, b);
            }
        }
        
        // Apply repair if enabled and we have confirmed defects
        if (repairParams.enable && !temporalResult.confirmed.empty()) {
            instanceData->fixer->repairDefects(outputBuffer, temporalResult.confirmed);
        }
        
    } catch (const std::exception& e) {
        DEBUG_LOG_ERROR(std::string("Render error: ") + e.what());
        DeadPixelQC_OFX::OfxUtil::reportError(effect, std::string("Render error: ") + e.what());
        status = kOfxStatFailed;
    }
    
    // Release images
    gEffectSuite->clipReleaseImage(sourceImg);
    gEffectSuite->clipReleaseImage(outputImg);
    
    return status;
}

// Plugin main entry point
static OfxStatus pluginMain(const char* action, 
                            const void* handle, 
                            OfxPropertySetHandle inArgs, 
                            OfxPropertySetHandle outArgs) {
    DEBUG_LOG_ACTION(action, "Plugin main entry");
    
    OfxImageEffectHandle effect = reinterpret_cast<OfxImageEffectHandle>(const_cast<void*>(handle));
    
    try {
        if (strcmp(action, kOfxActionLoad) == 0) {
            return onLoad();
        } else if (strcmp(action, kOfxActionUnload) == 0) {
            return onUnload();
        } else if (strcmp(action, kOfxActionDescribe) == 0) {
            return describe(effect);
        } else if (strcmp(action, kOfxImageEffectActionDescribeInContext) == 0) {
            return describeInContext(effect, inArgs);
        } else if (strcmp(action, kOfxActionCreateInstance) == 0) {
            return createInstance(effect);
        } else if (strcmp(action, kOfxActionDestroyInstance) == 0) {
            return destroyInstance(effect);
        } else if (strcmp(action, kOfxImageEffectActionGetClipPreferences) == 0) {
            return getClipPreferences(effect, inArgs, outArgs);
        } else if (strcmp(action, kOfxActionInstanceChanged) == 0) {
            return instanceChanged(effect, inArgs, outArgs);
        } else if (strcmp(action, kOfxImageEffectActionBeginSequenceRender) == 0) {
            return beginSequenceRender(effect, inArgs, outArgs);
        } else if (strcmp(action, kOfxImageEffectActionEndSequenceRender) == 0) {
            return endSequenceRender(effect, inArgs, outArgs);
        } else if (strcmp(action, kOfxImageEffectActionRender) == 0) {
            return render(effect, inArgs, outArgs);
        }
        
        // Unknown action
        DEBUG_LOG_WARNING(std::string("Unknown action: ") + action);
        return kOfxStatReplyDefault;
    } catch (const std::exception& e) {
        DEBUG_LOG_ERROR(std::string("Plugin error: ") + e.what());
        DeadPixelQC_OFX::OfxUtil::reportError(effect, std::string("Plugin error: ") + e.what());
        return kOfxStatFailed;
    }
}

// Set host function (required by OFX)
static void setHostFunction(OfxHost* host) {
    DEBUG_LOG("setHostFunction called");
    gHost = host;
    DEBUG_LOG("Host pointer stored");
}

// Optional OfxSetHost function (added in OFX 2020)
OfxExport OfxStatus OfxSetHost(const OfxHost* host) {
    DEBUG_LOG("OfxSetHost called (optional function)");
    gHost = host;
    return kOfxStatOK;
}

// OFX plugin entry point
OfxExport int OfxGetNumberOfPlugins(void) {
    DEBUG_LOG("OfxGetNumberOfPlugins called, returning 1");
    return 1;
}

OfxExport OfxPlugin* OfxGetPlugin(int nth) {
    DEBUG_LOG(std::string("OfxGetPlugin called with nth=") + std::to_string(nth));
    
    if (nth != 0) {
        DEBUG_LOG("Returning nullptr (nth != 0)");
        return nullptr;
    }
    
    static OfxPlugin plugin;
    plugin.pluginApi = kOfxImageEffectPluginApi;
    plugin.apiVersion = kOfxImageEffectPluginApiVersion;
    plugin.pluginIdentifier = "DeadPixelQC";
    plugin.pluginVersionMajor = 1;
    plugin.pluginVersionMinor = 0;
    plugin.setHost = setHostFunction;  // Set the required setHost function
    plugin.mainEntry = pluginMain;
    
    DEBUG_LOG("Returning plugin structure");
    return &plugin;
}
