// DeadPixelQC OFX Plugin
// Copyright (c) 2025

#include "ofxCore.h"
#include "ofxImageEffect.h"
#include "ofxMemory.h"
#include "ofxMultiThread.h"
#include "ofxMessage.h"
#include "ofxParam.h"
#include "Workers/SpatialWorker.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <new>
#include <string>

#ifndef DPQC_MINIMAL_UI
#define DPQC_MINIMAL_UI 1
#endif

#ifndef DPQC_ENABLE_SPATIAL
#define DPQC_ENABLE_SPATIAL 0
#endif

#ifndef DPQC_ENABLE_TEMPORAL
#define DPQC_ENABLE_TEMPORAL 0
#endif

#ifndef DPQC_ENABLE_FIX
#define DPQC_ENABLE_FIX 0
#endif

#define DPQC_LOG(msg) do { } while (0)
#define DPQC_LOG_ACTION(action, handle) do { } while (0)
#define DPQC_LOG_ERROR(msg) do { } while (0)
#define DPQC_LOG_WARNING(msg) do { } while (0)

// Global OFX host pointer
static const OfxHost* gHost = nullptr;

// Global OFX suite pointers
static const OfxImageEffectSuiteV1* gEffectSuite = nullptr;
static const OfxPropertySuiteV1* gPropertySuite = nullptr;
static const OfxParameterSuiteV1* gParamSuite = nullptr;
static const OfxMemorySuiteV1* gMemorySuite = nullptr;
static const OfxMultiThreadSuiteV1* gMultiThreadSuite = nullptr;
static const OfxMessageSuiteV1* gMessageSuite = nullptr;

namespace {
namespace OFXW = DeadPixelQC_OFX::Workers;

constexpr const char* kParamEnable = "Enable";
constexpr const char* kParamThreshold = "Threshold";
constexpr const char* kParamMode = "Mode";

struct PluginInstanceData {
    OfxImageClipHandle sourceClip = nullptr;
    OfxImageClipHandle outputClip = nullptr;
    std::mutex mutex;
};

struct ImageDesc {
    void* data = nullptr;
    int rowBytes = 0;
    OfxRectI bounds = {0, 0, 0, 0};
    const char* components = nullptr;
    const char* bitDepth = nullptr;
};

static OFXW::FramePixelFormat toWorkerFormat(const char* components, const char* bitDepth) {
    if (!components || !bitDepth) {
        return OFXW::FramePixelFormat::Unknown;
    }
    if (std::strcmp(components, kOfxImageComponentRGB) == 0 && std::strcmp(bitDepth, kOfxBitDepthByte) == 0) {
        return OFXW::FramePixelFormat::RGB8;
    }
    if (std::strcmp(components, kOfxImageComponentRGBA) == 0 && std::strcmp(bitDepth, kOfxBitDepthByte) == 0) {
        return OFXW::FramePixelFormat::RGBA8;
    }
    if (std::strcmp(components, kOfxImageComponentRGB) == 0 && std::strcmp(bitDepth, kOfxBitDepthShort) == 0) {
        return OFXW::FramePixelFormat::RGB16;
    }
    if (std::strcmp(components, kOfxImageComponentRGBA) == 0 && std::strcmp(bitDepth, kOfxBitDepthShort) == 0) {
        return OFXW::FramePixelFormat::RGBA16;
    }
    if (std::strcmp(components, kOfxImageComponentRGB) == 0 && std::strcmp(bitDepth, kOfxBitDepthFloat) == 0) {
        return OFXW::FramePixelFormat::RGB32F;
    }
    if (std::strcmp(components, kOfxImageComponentRGBA) == 0 && std::strcmp(bitDepth, kOfxBitDepthFloat) == 0) {
        return OFXW::FramePixelFormat::RGBA32F;
    }
    return OFXW::FramePixelFormat::Unknown;
}

static int componentsToCount(const char* components) {
    if (!components) {
        return 0;
    }
    if (std::strcmp(components, kOfxImageComponentRGBA) == 0) {
        return 4;
    }
    if (std::strcmp(components, kOfxImageComponentRGB) == 0) {
        return 3;
    }
    if (std::strcmp(components, kOfxImageComponentAlpha) == 0) {
        return 1;
    }
    return 0;
}

static int bitDepthToBytes(const char* bitDepth) {
    if (!bitDepth) {
        return 0;
    }
    if (std::strcmp(bitDepth, kOfxBitDepthByte) == 0) {
        return 1;
    }
    if (std::strcmp(bitDepth, kOfxBitDepthShort) == 0 || std::strcmp(bitDepth, kOfxBitDepthHalf) == 0) {
        return 2;
    }
    if (std::strcmp(bitDepth, kOfxBitDepthFloat) == 0) {
        return 4;
    }
    return 0;
}

static PluginInstanceData* getInstanceData(OfxImageEffectHandle effect) {
    OfxPropertySetHandle effectProps = nullptr;
    if (!gEffectSuite || !gPropertySuite) {
        return nullptr;
    }
    if (gEffectSuite->getPropertySet(effect, &effectProps) != kOfxStatOK || !effectProps) {
        return nullptr;
    }

    PluginInstanceData* instanceData = nullptr;
    if (gPropertySuite->propGetPointer(effectProps, kOfxPropInstanceData, 0,
                                       reinterpret_cast<void**>(&instanceData)) != kOfxStatOK) {
        return nullptr;
    }
    return instanceData;
}

static void setInstanceData(OfxImageEffectHandle effect, PluginInstanceData* instanceData) {
    OfxPropertySetHandle effectProps = nullptr;
    if (!gEffectSuite || !gPropertySuite) {
        return;
    }
    if (gEffectSuite->getPropertySet(effect, &effectProps) != kOfxStatOK || !effectProps) {
        return;
    }
    gPropertySuite->propSetPointer(effectProps, kOfxPropInstanceData, 0, instanceData);
}

static bool fetchImageDesc(OfxPropertySetHandle imageHandle, ImageDesc& out) {
    if (!imageHandle || !gPropertySuite) {
        return false;
    }

    char* components = nullptr;
    char* bitDepth = nullptr;

    if (gPropertySuite->propGetPointer(imageHandle, kOfxImagePropData, 0, &out.data) != kOfxStatOK || !out.data) {
        return false;
    }
    if (gPropertySuite->propGetInt(imageHandle, kOfxImagePropRowBytes, 0, &out.rowBytes) != kOfxStatOK || out.rowBytes == 0) {
        return false;
    }
    if (gPropertySuite->propGetIntN(imageHandle, kOfxImagePropBounds, 4, &out.bounds.x1) != kOfxStatOK) {
        return false;
    }
    if (gPropertySuite->propGetString(imageHandle, kOfxImageEffectPropComponents, 0, &components) != kOfxStatOK) {
        return false;
    }
    if (gPropertySuite->propGetString(imageHandle, kOfxImageEffectPropPixelDepth, 0, &bitDepth) != kOfxStatOK) {
        return false;
    }

    out.components = components;
    out.bitDepth = bitDepth;
    return true;
}

static void copyImageRows(const ImageDesc& src, const ImageDesc& dst) {
    const int srcWidth = src.bounds.x2 - src.bounds.x1;
    const int srcHeight = src.bounds.y2 - src.bounds.y1;
    const int dstWidth = dst.bounds.x2 - dst.bounds.x1;
    const int dstHeight = dst.bounds.y2 - dst.bounds.y1;

    const int width = std::min(srcWidth, dstWidth);
    const int height = std::min(srcHeight, dstHeight);
    if (width <= 0 || height <= 0) {
        return;
    }

    const int componentCount = componentsToCount(src.components);
    const int bytesPerComponent = bitDepthToBytes(src.bitDepth);
    if (componentCount <= 0 || bytesPerComponent <= 0) {
        return;
    }

    const int bytesPerPixel = componentCount * bytesPerComponent;
    const int srcStride = (src.rowBytes < 0) ? -src.rowBytes : src.rowBytes;
    const int dstStride = (dst.rowBytes < 0) ? -dst.rowBytes : dst.rowBytes;
    if (srcStride <= 0 || dstStride <= 0) {
        return;
    }

    const int requestedBytes = width * bytesPerPixel;
    const int bytesToCopy = std::min(requestedBytes, std::min(srcStride, dstStride));
    if (bytesToCopy <= 0) {
        return;
    }

    const auto* srcBase = static_cast<const std::uint8_t*>(src.data);
    auto* dstBase = static_cast<std::uint8_t*>(dst.data);

    if (src.rowBytes < 0) {
        srcBase += static_cast<std::ptrdiff_t>(srcStride) * (height - 1);
    }
    if (dst.rowBytes < 0) {
        dstBase += static_cast<std::ptrdiff_t>(dstStride) * (height - 1);
    }

    for (int y = 0; y < height; ++y) {
        const std::uint8_t* srcRow = (src.rowBytes < 0) ? (srcBase - static_cast<std::ptrdiff_t>(srcStride) * y)
                                                        : (srcBase + static_cast<std::ptrdiff_t>(srcStride) * y);
        std::uint8_t* dstRow = (dst.rowBytes < 0) ? (dstBase - static_cast<std::ptrdiff_t>(dstStride) * y)
                                                  : (dstBase + static_cast<std::ptrdiff_t>(dstStride) * y);
        std::memcpy(dstRow, srcRow, static_cast<size_t>(bytesToCopy));
    }
}

static OfxStatus fetchHostSuites() {
    DPQC_LOG("Fetching host suites");

    if (!gHost || !gHost->fetchSuite) {
        DPQC_LOG_ERROR("Host or fetchSuite is null");
        return kOfxStatErrMissingHostFeature;
    }

    gEffectSuite = reinterpret_cast<const OfxImageEffectSuiteV1*>(
        gHost->fetchSuite(gHost->host, kOfxImageEffectSuite, 1));

    gPropertySuite = reinterpret_cast<const OfxPropertySuiteV1*>(
        gHost->fetchSuite(gHost->host, kOfxPropertySuite, 1));

    gParamSuite = reinterpret_cast<const OfxParameterSuiteV1*>(
        gHost->fetchSuite(gHost->host, kOfxParameterSuite, 1));

    // Optional suites: absence must not fail plugin load.
    gMemorySuite = reinterpret_cast<const OfxMemorySuiteV1*>(
        gHost->fetchSuite(gHost->host, kOfxMemorySuite, 1));

    gMultiThreadSuite = reinterpret_cast<const OfxMultiThreadSuiteV1*>(
        gHost->fetchSuite(gHost->host, kOfxMultiThreadSuite, 1));

    gMessageSuite = reinterpret_cast<const OfxMessageSuiteV1*>(
        gHost->fetchSuite(gHost->host, kOfxMessageSuite, 1));

    if (!gEffectSuite || !gPropertySuite || !gParamSuite) {
        DPQC_LOG_ERROR("Failed to fetch mandatory suites (ImageEffect/Property/Parameter)");
        return kOfxStatErrMissingHostFeature;
    }

    if (!gMemorySuite) {
        DPQC_LOG_WARNING("Optional suite missing: OfxMemorySuite");
    }
    if (!gMultiThreadSuite) {
        DPQC_LOG_WARNING("Optional suite missing: OfxMultiThreadSuite");
    }
    if (!gMessageSuite) {
        DPQC_LOG_WARNING("Optional suite missing: OfxMessageSuite");
    }

    return kOfxStatOK;
}

static OfxStatus onLoad() {
    DPQC_LOG_ACTION(kOfxActionLoad, "");
    return fetchHostSuites();
}

static OfxStatus onUnload() {
    DPQC_LOG_ACTION(kOfxActionUnload, "");

    gEffectSuite = nullptr;
    gPropertySuite = nullptr;
    gParamSuite = nullptr;
    gMemorySuite = nullptr;
    gMultiThreadSuite = nullptr;
    gMessageSuite = nullptr;

    return kOfxStatOK;
}

static bool getParamBoolAtTime(OfxParamSetHandle paramSet, const char* name, double time, bool fallback) {
    if (!paramSet || !gParamSuite) {
        return fallback;
    }
    OfxParamHandle handle = nullptr;
    if (gParamSuite->paramGetHandle(paramSet, name, &handle, nullptr) != kOfxStatOK || !handle) {
        return fallback;
    }
    int value = fallback ? 1 : 0;
    if (gParamSuite->paramGetValueAtTime(handle, time, &value) != kOfxStatOK) {
        return fallback;
    }
    return value != 0;
}

static double getParamDoubleAtTime(OfxParamSetHandle paramSet, const char* name, double time, double fallback) {
    if (!paramSet || !gParamSuite) {
        return fallback;
    }
    OfxParamHandle handle = nullptr;
    if (gParamSuite->paramGetHandle(paramSet, name, &handle, nullptr) != kOfxStatOK || !handle) {
        return fallback;
    }
    double value = fallback;
    if (gParamSuite->paramGetValueAtTime(handle, time, &value) != kOfxStatOK) {
        return fallback;
    }
    return value;
}

static int getParamIntAtTime(OfxParamSetHandle paramSet, const char* name, double time, int fallback) {
    if (!paramSet || !gParamSuite) {
        return fallback;
    }
    OfxParamHandle handle = nullptr;
    if (gParamSuite->paramGetHandle(paramSet, name, &handle, nullptr) != kOfxStatOK || !handle) {
        return fallback;
    }
    int value = fallback;
    if (gParamSuite->paramGetValueAtTime(handle, time, &value) != kOfxStatOK) {
        return fallback;
    }
    return value;
}

static void defineMinimalParameters(OfxParamSetHandle paramSet) {
    OfxPropertySetHandle paramProps = nullptr;

    gParamSuite->paramDefine(paramSet, kOfxParamTypeBoolean, kParamEnable, &paramProps);
    gPropertySuite->propSetInt(paramProps, kOfxParamPropDefault, 0, 1);
    gPropertySuite->propSetString(paramProps, kOfxPropLabel, 0, "Enable");

    gParamSuite->paramDefine(paramSet, kOfxParamTypeDouble, kParamThreshold, &paramProps);
    gPropertySuite->propSetDouble(paramProps, kOfxParamPropDefault, 0, 0.98);
    gPropertySuite->propSetDouble(paramProps, kOfxParamPropMin, 0, 0.0);
    gPropertySuite->propSetDouble(paramProps, kOfxParamPropMax, 0, 1.0);
    gPropertySuite->propSetString(paramProps, kOfxPropLabel, 0, "Threshold");

    gParamSuite->paramDefine(paramSet, kOfxParamTypeChoice, kParamMode, &paramProps);
    gPropertySuite->propSetString(paramProps, kOfxParamPropChoiceOption, 0, "PassThrough");
    gPropertySuite->propSetString(paramProps, kOfxParamPropChoiceOption, 1, "CandidatesOverlay");
    gPropertySuite->propSetString(paramProps, kOfxParamPropChoiceOption, 2, "MaskOnly");
    gPropertySuite->propSetInt(paramProps, kOfxParamPropDefault, 0, 0);
    gPropertySuite->propSetString(paramProps, kOfxPropLabel, 0, "Mode");
}

static OfxStatus describe(OfxImageEffectHandle effect) {
    DPQC_LOG_ACTION(kOfxActionDescribe, "Describing plugin");

    OfxPropertySetHandle effectProps = nullptr;
    if (gEffectSuite->getPropertySet(effect, &effectProps) != kOfxStatOK || !effectProps) {
        return kOfxStatFailed;
    }

    gPropertySuite->propSetString(effectProps, kOfxPropLabel, 0, "DeadPixelQC");
    gPropertySuite->propSetString(effectProps, kOfxImageEffectPluginPropGrouping, 0, "Quality Control");

    gPropertySuite->propSetString(effectProps, kOfxImageEffectPropSupportedContexts, 0, kOfxImageEffectContextFilter);

    gPropertySuite->propSetString(effectProps, kOfxImageEffectPropSupportedPixelDepths, 0, kOfxBitDepthByte);
    gPropertySuite->propSetString(effectProps, kOfxImageEffectPropSupportedPixelDepths, 1, kOfxBitDepthShort);
    gPropertySuite->propSetString(effectProps, kOfxImageEffectPropSupportedPixelDepths, 2, kOfxBitDepthFloat);

    gPropertySuite->propSetInt(effectProps, kOfxImageEffectPluginPropHostFrameThreading, 0, 1);
    gPropertySuite->propSetString(effectProps, kOfxImageEffectPluginRenderThreadSafety, 0, kOfxImageEffectRenderInstanceSafe);

    return kOfxStatOK;
}

static OfxStatus describeInContext(OfxImageEffectHandle effect, OfxPropertySetHandle /*inArgs*/) {
    DPQC_LOG_ACTION(kOfxImageEffectActionDescribeInContext, "Describing in context");

    OfxPropertySetHandle clipProps = nullptr;

    if (gEffectSuite->clipDefine(effect, kOfxImageEffectOutputClipName, &clipProps) == kOfxStatOK && clipProps) {
        gPropertySuite->propSetString(clipProps, kOfxImageEffectPropSupportedComponents, 0, kOfxImageComponentRGBA);
        gPropertySuite->propSetString(clipProps, kOfxImageEffectPropSupportedComponents, 1, kOfxImageComponentRGB);
    }

    if (gEffectSuite->clipDefine(effect, kOfxImageEffectSimpleSourceClipName, &clipProps) == kOfxStatOK && clipProps) {
        gPropertySuite->propSetString(clipProps, kOfxImageEffectPropSupportedComponents, 0, kOfxImageComponentRGBA);
        gPropertySuite->propSetString(clipProps, kOfxImageEffectPropSupportedComponents, 1, kOfxImageComponentRGB);
    }

    OfxParamSetHandle paramSet = nullptr;
    if (gEffectSuite->getParamSet(effect, &paramSet) == kOfxStatOK && paramSet) {
        defineMinimalParameters(paramSet);
    }

    return kOfxStatOK;
}

static OfxStatus createInstance(OfxImageEffectHandle effect) {
    DPQC_LOG_ACTION(kOfxActionCreateInstance, "Creating instance");

    auto* instanceData = new (std::nothrow) PluginInstanceData();
    if (!instanceData) {
        return kOfxStatErrMemory;
    }

    if (gEffectSuite->clipGetHandle(effect, kOfxImageEffectSimpleSourceClipName, &instanceData->sourceClip, nullptr) != kOfxStatOK) {
        delete instanceData;
        return kOfxStatErrBadHandle;
    }

    if (gEffectSuite->clipGetHandle(effect, kOfxImageEffectOutputClipName, &instanceData->outputClip, nullptr) != kOfxStatOK) {
        delete instanceData;
        return kOfxStatErrBadHandle;
    }

    setInstanceData(effect, instanceData);
    return kOfxStatOK;
}

static OfxStatus destroyInstance(OfxImageEffectHandle effect) {
    DPQC_LOG_ACTION(kOfxActionDestroyInstance, "Destroying instance");

    PluginInstanceData* instanceData = getInstanceData(effect);
    if (instanceData) {
        delete instanceData;
        setInstanceData(effect, nullptr);
    }
    return kOfxStatOK;
}

static OfxStatus getClipPreferences(OfxImageEffectHandle /*effect*/,
                                    OfxPropertySetHandle /*inArgs*/,
                                    OfxPropertySetHandle outArgs) {
    DPQC_LOG_ACTION(kOfxImageEffectActionGetClipPreferences, "Getting clip preferences");

    if (outArgs) {
        gPropertySuite->propSetString(outArgs, "OfxImageClipPropComponents_Output", 0, kOfxImageComponentRGBA);
    }
    return kOfxStatOK;
}

static OfxStatus render(OfxImageEffectHandle effect,
                        OfxPropertySetHandle inArgs,
                        OfxPropertySetHandle /*outArgs*/) {
    DPQC_LOG_ACTION(kOfxImageEffectActionRender, "Rendering frame");

    PluginInstanceData* instanceData = getInstanceData(effect);
    if (!instanceData) {
        return kOfxStatErrBadHandle;
    }

    double time = 0.0;
    if (inArgs) {
        gPropertySuite->propGetDouble(inArgs, kOfxPropTime, 0, &time);
    }

    OfxPropertySetHandle sourceImg = nullptr;
    OfxPropertySetHandle outputImg = nullptr;

    if (gEffectSuite->clipGetImage(instanceData->sourceClip, time, nullptr, &sourceImg) != kOfxStatOK || !sourceImg) {
        return kOfxStatOK;
    }

    if (gEffectSuite->clipGetImage(instanceData->outputClip, time, nullptr, &outputImg) != kOfxStatOK || !outputImg) {
        gEffectSuite->clipReleaseImage(sourceImg);
        return kOfxStatOK;
    }

    OfxStatus status = kOfxStatOK;
    try {
        ImageDesc srcDesc;
        ImageDesc dstDesc;

        if (fetchImageDesc(sourceImg, srcDesc) && fetchImageDesc(outputImg, dstDesc)) {
            const bool sameDepth = srcDesc.bitDepth && dstDesc.bitDepth && std::strcmp(srcDesc.bitDepth, dstDesc.bitDepth) == 0;
            const bool sameComponents = srcDesc.components && dstDesc.components && std::strcmp(srcDesc.components, dstDesc.components) == 0;
            if (sameDepth && sameComponents) {
                const int srcWidth = srcDesc.bounds.x2 - srcDesc.bounds.x1;
                const int srcHeight = srcDesc.bounds.y2 - srcDesc.bounds.y1;
                const int dstWidth = dstDesc.bounds.x2 - dstDesc.bounds.x1;
                const int dstHeight = dstDesc.bounds.y2 - dstDesc.bounds.y1;

                OFXW::FrameView inView;
                inView.data = srcDesc.data;
                inView.width = srcWidth;
                inView.height = srcHeight;
                inView.rowBytes = srcDesc.rowBytes;
                inView.format = toWorkerFormat(srcDesc.components, srcDesc.bitDepth);

                OFXW::FrameView outView;
                outView.data = dstDesc.data;
                outView.width = dstWidth;
                outView.height = dstHeight;
                outView.rowBytes = dstDesc.rowBytes;
                outView.format = toWorkerFormat(dstDesc.components, dstDesc.bitDepth);

#if DPQC_ENABLE_SPATIAL
                OfxParamSetHandle paramSet = nullptr;
                const bool hasParamSet = (gEffectSuite->getParamSet(effect, &paramSet) == kOfxStatOK && paramSet != nullptr);

                OFXW::SpatialParams workerParams;
                workerParams.enable = getParamBoolAtTime(paramSet, kParamEnable, time, true);
                workerParams.lumaThreshold = static_cast<float>(getParamDoubleAtTime(paramSet, kParamThreshold, time, 0.98));
                workerParams.whitenessThreshold = 0.05f;
                workerParams.robustZ = 10.0f;
                workerParams.maxClusterArea = 4;

                const int mode = getParamIntAtTime(paramSet, kParamMode, time, 0);
                if (!hasParamSet || mode <= 0) {
                    workerParams.outputMode = OFXW::SpatialOutputMode::PassThrough;
                } else if (mode == 1) {
                    workerParams.outputMode = OFXW::SpatialOutputMode::CandidatesOverlay;
                } else {
                    workerParams.outputMode = OFXW::SpatialOutputMode::MaskOnly;
                }

                OFXW::SpatialWorker worker;
                const OFXW::SpatialResult workerResult = worker.process(inView, outView, workerParams);
                if (!workerResult.ok) {
                    copyImageRows(srcDesc, dstDesc);
                }
#else
                copyImageRows(srcDesc, dstDesc);
#endif
            }
        }
#if DPQC_ENABLE_FIX
        // Reserved for stepwise enabling of FixWorker path.
#endif
#if DPQC_ENABLE_TEMPORAL
        // Temporal path intentionally deferred (stateful, order-sensitive in host threading).
#endif

    } catch (...) {
        status = kOfxStatOK;
    }

    gEffectSuite->clipReleaseImage(sourceImg);
    gEffectSuite->clipReleaseImage(outputImg);
    return status;
}

static OfxStatus pluginMain(const char* action,
                            const void* handle,
                            OfxPropertySetHandle inArgs,
                            OfxPropertySetHandle outArgs) {
    DPQC_LOG_ACTION(action ? action : "<null>", "Plugin main entry");

    OfxImageEffectHandle effect = reinterpret_cast<OfxImageEffectHandle>(const_cast<void*>(handle));

    if (!action) {
        return kOfxStatReplyDefault;
    }

    if (std::strcmp(action, kOfxActionLoad) == 0) {
        return onLoad();
    }
    if (std::strcmp(action, kOfxActionUnload) == 0) {
        return onUnload();
    }
    if (std::strcmp(action, kOfxActionDescribe) == 0) {
        return describe(effect);
    }
    if (std::strcmp(action, kOfxImageEffectActionDescribeInContext) == 0) {
        return describeInContext(effect, inArgs);
    }
    if (std::strcmp(action, kOfxActionCreateInstance) == 0) {
        return createInstance(effect);
    }
    if (std::strcmp(action, kOfxActionDestroyInstance) == 0) {
        return destroyInstance(effect);
    }
    if (std::strcmp(action, kOfxImageEffectActionGetClipPreferences) == 0) {
        return getClipPreferences(effect, inArgs, outArgs);
    }
    if (std::strcmp(action, kOfxImageEffectActionRender) == 0) {
        return render(effect, inArgs, outArgs);
    }

    return kOfxStatReplyDefault;
}

static void setHostFunction(OfxHost* host) {
    gHost = host;
}

} // namespace

extern "C" OfxExport OfxStatus OfxSetHost(const OfxHost* host) {
    gHost = host;
    return kOfxStatOK;
}

extern "C" OfxExport int OfxGetNumberOfPlugins(void) {
    return 1;
}

extern "C" OfxExport OfxPlugin* OfxGetPlugin(int nth) {
    if (nth != 0) {
        return nullptr;
    }

    static OfxPlugin plugin;
    plugin.pluginApi = kOfxImageEffectPluginApi;
    plugin.apiVersion = kOfxImageEffectPluginApiVersion;
    plugin.pluginIdentifier = "DeadPixelQC";
    plugin.pluginVersionMajor = 1;
    plugin.pluginVersionMinor = 0;
    plugin.setHost = setHostFunction;
    plugin.mainEntry = pluginMain;

    return &plugin;
}
