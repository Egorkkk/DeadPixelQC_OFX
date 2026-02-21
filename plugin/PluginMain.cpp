// DeadPixelQC OFX Plugin
// Copyright (c) 2025

#include "ofxCore.h"
#include "ofxImageEffect.h"
#include "ofxMemory.h"
#include "ofxMultiThread.h"
#include "ofxMessage.h"
#include "ofxParam.h"
#include "Workers/SpatialWorker.h"
#include "PluginParams.h"
#include "OfxUtil.h"
#include "../core/Fixer.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <mutex>
#include <memory>
#include <new>
#include <string>
#include <sstream>
#include <vector>

#ifndef DPQC_MINIMAL_UI
#define DPQC_MINIMAL_UI 1
#endif

#if !DPQC_MINIMAL_UI
#include "../log/EventLogger.h"
#include "../log/MapAccumulator.h"
#include "../model/DefectMap.h"
#include "../export/ExportCSV.h"
#include "../export/ExportDefectMap.h"
#include "../export/ExportJSON.h"
#include "../export/ImportDefectMap.h"
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

namespace {
namespace OFXW = DeadPixelQC_OFX::Workers;

const OfxImageEffectSuiteV1*& gEffectSuite = DeadPixelQC_OFX::gEffectSuite;
const OfxPropertySuiteV1*& gPropertySuite = DeadPixelQC_OFX::gPropertySuite;
const OfxParameterSuiteV1*& gParamSuite = DeadPixelQC_OFX::gParamSuite;
const OfxMemorySuiteV1*& gMemorySuite = DeadPixelQC_OFX::gMemorySuite;
const OfxMultiThreadSuiteV1*& gMultiThreadSuite = DeadPixelQC_OFX::gMultiThreadSuite;
const OfxMessageSuiteV1*& gMessageSuite = DeadPixelQC_OFX::gMessageSuite;

constexpr const char* kParamEnable = "Enable";
constexpr const char* kParamThreshold = "Threshold";
constexpr const char* kParamMode = "Mode";

struct PluginInstanceData {
    OfxImageClipHandle sourceClip = nullptr;
    OfxImageClipHandle outputClip = nullptr;
    std::mutex mutex;
    
#if !DPQC_MINIMAL_UI
    std::unique_ptr<DeadPixelQC::EventLogger> eventLogger;
    DeadPixelQC::MapAccumulator mapAccumulator;
    DeadPixelQC::DefectMap defectMap;
    int selectedEventIndex = -1;
    std::atomic<bool> suppressInstanceChanged { false };
#endif
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

static DeadPixelQC::PixelFormat toCoreFormat(const char* components, const char* bitDepth) {
    if (!components || !bitDepth) {
        return DeadPixelQC::PixelFormat::Unknown;
    }
    if (std::strcmp(components, kOfxImageComponentRGB) == 0 && std::strcmp(bitDepth, kOfxBitDepthByte) == 0) {
        return DeadPixelQC::PixelFormat::RGB8;
    }
    if (std::strcmp(components, kOfxImageComponentRGBA) == 0 && std::strcmp(bitDepth, kOfxBitDepthByte) == 0) {
        return DeadPixelQC::PixelFormat::RGBA8;
    }
    if (std::strcmp(components, kOfxImageComponentRGB) == 0 && std::strcmp(bitDepth, kOfxBitDepthShort) == 0) {
        return DeadPixelQC::PixelFormat::RGB16;
    }
    if (std::strcmp(components, kOfxImageComponentRGBA) == 0 && std::strcmp(bitDepth, kOfxBitDepthShort) == 0) {
        return DeadPixelQC::PixelFormat::RGBA16;
    }
    if (std::strcmp(components, kOfxImageComponentRGB) == 0 && std::strcmp(bitDepth, kOfxBitDepthFloat) == 0) {
        return DeadPixelQC::PixelFormat::RGB32F;
    }
    if (std::strcmp(components, kOfxImageComponentRGBA) == 0 && std::strcmp(bitDepth, kOfxBitDepthFloat) == 0) {
        return DeadPixelQC::PixelFormat::RGBA32F;
    }
    return DeadPixelQC::PixelFormat::Unknown;
}

#if !DPQC_MINIMAL_UI
static std::string formatMapSize(std::size_t size) {
    return std::to_string(static_cast<unsigned long long>(size));
}

static std::string currentUtcIso8601() {
    const auto now = std::chrono::system_clock::now();
    const std::time_t timeNow = std::chrono::system_clock::to_time_t(now);
    std::tm utc{};
#if defined(_WIN32)
    gmtime_s(&utc, &timeNow);
#else
    gmtime_r(&timeNow, &utc);
#endif

    char buffer[32];
    const std::size_t count = std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", &utc);
    return (count > 0) ? std::string(buffer, count) : std::string();
}

static DeadPixelQC::DetectionHitList toDetectionHits(const DeadPixelQC::FrameDetection& detection) {
    DeadPixelQC::DetectionHitList hits;
    hits.reserve(detection.candidates.size());
    for (const auto& candidate : detection.candidates) {
        if (!candidate.pixels.empty()) {
            const DeadPixelQC::PixelCoord& p = candidate.pixels.front();
            hits.push_back({p.x, p.y, 0.5f});
        }
    }
    return hits;
}

static int mapNormalizedToPixel(float value, int size) {
    if (size <= 1) {
        return 0;
    }
    const double clamped = std::max(0.0, std::min(1.0, static_cast<double>(value)));
    return static_cast<int>(std::lround(clamped * static_cast<double>(size - 1)));
}

static OFXW::SpatialOutputMode resolveSpatialOutputMode(OfxParamSetHandle paramSet, double time) {
    const bool hasViewMode = (paramSet != nullptr) && (gParamSuite != nullptr) &&
        [&]() {
            OfxParamHandle h = nullptr;
            return gParamSuite->paramGetHandle(paramSet, DeadPixelQC_OFX::PARAM_VIEW_MODE, &h, nullptr) == kOfxStatOK && h != nullptr;
        }();

    if (hasViewMode) {
        const int viewMode = [&]() {
            int v = 0;
            OfxParamHandle h = nullptr;
            if (gParamSuite->paramGetHandle(paramSet, DeadPixelQC_OFX::PARAM_VIEW_MODE, &h, nullptr) == kOfxStatOK && h) {
                gParamSuite->paramGetValueAtTime(h, time, &v);
            }
            return v;
        }();
        if (viewMode == 3) {
            return OFXW::SpatialOutputMode::MaskOnly;
        }
        if (viewMode == 1 || viewMode == 2) {
            return OFXW::SpatialOutputMode::CandidatesOverlay;
        }
        return OFXW::SpatialOutputMode::PassThrough;
    }

    int mode = 0;
    if (paramSet && gParamSuite) {
        OfxParamHandle h = nullptr;
        if (gParamSuite->paramGetHandle(paramSet, kParamMode, &h, nullptr) == kOfxStatOK && h) {
            gParamSuite->paramGetValueAtTime(h, time, &mode);
        }
    }
    if (mode == 2) {
        return OFXW::SpatialOutputMode::MaskOnly;
    }
    if (mode == 1) {
        return OFXW::SpatialOutputMode::CandidatesOverlay;
    }
    return OFXW::SpatialOutputMode::PassThrough;
}
#endif

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

static std::string getParamStringAtTime(OfxParamSetHandle paramSet, const char* name, double time, const std::string& fallback) {
    if (!paramSet || !gParamSuite) {
        return fallback;
    }
    OfxParamHandle handle = nullptr;
    if (gParamSuite->paramGetHandle(paramSet, name, &handle, nullptr) != kOfxStatOK || !handle) {
        return fallback;
    }
    char* value = nullptr;
    if (gParamSuite->paramGetValueAtTime(handle, time, &value) != kOfxStatOK || !value) {
        return fallback;
    }
    return std::string(value);
}

static bool hasParam(OfxParamSetHandle paramSet, const char* name) {
    if (!paramSet || !gParamSuite || !name) {
        return false;
    }
    OfxParamHandle handle = nullptr;
    return gParamSuite->paramGetHandle(paramSet, name, &handle, nullptr) == kOfxStatOK && handle != nullptr;
}

static void setParamString(OfxParamSetHandle paramSet, const char* name, const std::string& value) {
    if (!paramSet || !gParamSuite) {
        return;
    }
    OfxParamHandle handle = nullptr;
    if (gParamSuite->paramGetHandle(paramSet, name, &handle, nullptr) != kOfxStatOK || !handle) {
        return;
    }
    gParamSuite->paramSetValueAtTime(handle, 0.0, value.c_str());
}

static void setParamInt(OfxParamSetHandle paramSet, const char* name, int value) {
    if (!paramSet || !gParamSuite) {
        return;
    }
    OfxParamHandle handle = nullptr;
    if (gParamSuite->paramGetHandle(paramSet, name, &handle, nullptr) != kOfxStatOK || !handle) {
        return;
    }
    gParamSuite->paramSetValueAtTime(handle, 0.0, value);
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
#if !DPQC_MINIMAL_UI
        DeadPixelQC_OFX::defineWorkflowParameters(paramSet);
#endif
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

#if !DPQC_MINIMAL_UI
    instanceData->eventLogger = std::make_unique<DeadPixelQC::EventLogger>();
#endif

    setInstanceData(effect, instanceData);
    return kOfxStatOK;
}

static OfxStatus destroyInstance(OfxImageEffectHandle effect) {
    DPQC_LOG_ACTION(kOfxActionDestroyInstance, "Destroying instance");

    PluginInstanceData* instanceData = getInstanceData(effect);
    if (instanceData) {
#if !DPQC_MINIMAL_UI
        instanceData->eventLogger.reset();
#endif
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

#if !DPQC_MINIMAL_UI
static OfxStatus instanceChanged(OfxImageEffectHandle effect,
                                 OfxPropertySetHandle inArgs) {
    PluginInstanceData* instanceData = getInstanceData(effect);
    if (!instanceData) {
        return kOfxStatOK;
    }

    // Get the changed parameter name
    char* paramName = nullptr;
    if (inArgs && gPropertySuite) {
        gPropertySuite->propGetString(inArgs, kOfxPropName, 0, &paramName);
    }

    if (instanceData->suppressInstanceChanged.load(std::memory_order_acquire)) {
        return kOfxStatOK;
    }

    if (paramName) {
        OfxParamSetHandle paramSet = nullptr;
        if (gEffectSuite->getParamSet(effect, &paramSet) != kOfxStatOK || !paramSet) {
            return kOfxStatOK;
        }

        bool setCount = false;
        bool setIndex = false;
        bool setInfo = false;
        bool setExportStatus = false;
        bool setMapSize = false;
        bool setMapStatus = false;
        std::string nextCount;
        int nextIndex = -1;
        std::string nextInfo;
        std::string nextExportStatus;
        std::string nextMapSize;
        std::string nextMapStatus;
        bool runExport = false;
        bool runSaveMap = false;
        bool runLoadMap = false;
        std::string exportPath;
        std::string mapPath;
        int exportFormat = DeadPixelQC_OFX::DEFAULT_EXPORT_FORMAT;
        DeadPixelQC::EventModel exportSnapshot;
        DeadPixelQC::DefectMap mapSnapshot;
        DeadPixelQC::DefectMap loadedMap;

        {
            std::lock_guard<std::mutex> lock(instanceData->mutex);

            if (instanceData->suppressInstanceChanged.load(std::memory_order_relaxed)) {
                return kOfxStatOK;
            }

            // Handle button events
            if (std::strcmp(paramName, DeadPixelQC_OFX::PARAM_CLEAR_EVENTS) == 0) {
                if (instanceData->eventLogger && instanceData->eventLogger->model().size() > 0) {
                    instanceData->eventLogger->clear();
                    instanceData->selectedEventIndex = -1;
                    setCount = true;
                    setIndex = true;
                    setInfo = true;
                    nextCount = "0";
                    nextIndex = -1;
                    nextInfo = "None";
                }
            }
            else if (std::strcmp(paramName, DeadPixelQC_OFX::PARAM_PREV_EVENT) == 0) {
                const std::size_t eventCount = instanceData->eventLogger ? instanceData->eventLogger->model().size() : 0;
                if (eventCount > 0) {
                    int newIndex = instanceData->selectedEventIndex - 1;
                    if (newIndex < 0) newIndex = static_cast<int>(eventCount) - 1;

                    instanceData->selectedEventIndex = newIndex;
                    const auto& event = instanceData->eventLogger->model().get(newIndex);

                    std::ostringstream info;
                    if (!event.hits.empty()) {
                        const auto& hit = event.hits[0];
                        info << "frame=" << event.frameIndex << " x=" << hit.x << " y=" << hit.y
                             << " conf=" << hit.confidence << " pers=" << event.persistence;
                    }

                    setIndex = true;
                    setInfo = true;
                    nextIndex = newIndex;
                    nextInfo = info.str();
                }
            }
            else if (std::strcmp(paramName, DeadPixelQC_OFX::PARAM_NEXT_EVENT) == 0) {
                const std::size_t eventCount = instanceData->eventLogger ? instanceData->eventLogger->model().size() : 0;
                if (eventCount > 0) {
                    int newIndex = instanceData->selectedEventIndex + 1;
                    if (newIndex >= static_cast<int>(eventCount)) newIndex = 0;

                    instanceData->selectedEventIndex = newIndex;
                    const auto& event = instanceData->eventLogger->model().get(newIndex);

                    std::ostringstream info;
                    if (!event.hits.empty()) {
                        const auto& hit = event.hits[0];
                        info << "frame=" << event.frameIndex << " x=" << hit.x << " y=" << hit.y
                             << " conf=" << hit.confidence << " pers=" << event.persistence;
                    }

                    setIndex = true;
                    setInfo = true;
                    nextIndex = newIndex;
                    nextInfo = info.str();
                }
            }
            else if (std::strcmp(paramName, DeadPixelQC_OFX::PARAM_EXPORT_EVENTS) == 0) {
                exportPath = getParamStringAtTime(
                    paramSet,
                    DeadPixelQC_OFX::PARAM_EXPORT_PATH,
                    0.0,
                    DeadPixelQC_OFX::DEFAULT_EXPORT_PATH);
                exportFormat = getParamIntAtTime(
                    paramSet,
                    DeadPixelQC_OFX::PARAM_EXPORT_FORMAT,
                    0.0,
                    DeadPixelQC_OFX::DEFAULT_EXPORT_FORMAT);

                if (instanceData->eventLogger) {
                    const DeadPixelQC::EventModel& sourceModel = instanceData->eventLogger->model();
                    for (std::size_t i = 0; i < sourceModel.size(); ++i) {
                        exportSnapshot.addEvent(sourceModel.get(i));
                    }
                }
                runExport = true;
            }
            else if (std::strcmp(paramName, DeadPixelQC_OFX::PARAM_RESET_MAP) == 0) {
                instanceData->mapAccumulator.reset();
                instanceData->defectMap.clear();
                setMapSize = true;
                setMapStatus = true;
                nextMapSize = "0";
                nextMapStatus = "Empty";
            }
            else if (std::strcmp(paramName, DeadPixelQC_OFX::PARAM_FINALIZE_MAP) == 0) {
                DeadPixelQC::DefectMap finalized = instanceData->mapAccumulator.finalize(1);
                finalized.metadata().version = 1;
                finalized.metadata().createdUtc = currentUtcIso8601();
                finalized.metadata().cameraTag = getParamStringAtTime(
                    paramSet,
                    DeadPixelQC_OFX::PARAM_CAMERA_TAG,
                    0.0,
                    DeadPixelQC_OFX::DEFAULT_CAMERA_TAG);
                instanceData->defectMap = finalized;

                setMapSize = true;
                setMapStatus = true;
                nextMapSize = formatMapSize(finalized.size());
                std::ostringstream status;
                status << "Finalized: " << finalized.size() << " points";
                nextMapStatus = status.str();
            }
            else if (std::strcmp(paramName, DeadPixelQC_OFX::PARAM_SAVE_MAP) == 0) {
                mapPath = getParamStringAtTime(
                    paramSet,
                    DeadPixelQC_OFX::PARAM_MAP_PATH,
                    0.0,
                    DeadPixelQC_OFX::DEFAULT_MAP_PATH);
                if (!mapPath.empty()) {
                    mapSnapshot = instanceData->defectMap;
                    runSaveMap = true;
                } else {
                    setMapStatus = true;
                    nextMapStatus = "Save failed: empty path";
                }
            }
            else if (std::strcmp(paramName, DeadPixelQC_OFX::PARAM_LOAD_MAP) == 0) {
                mapPath = getParamStringAtTime(
                    paramSet,
                    DeadPixelQC_OFX::PARAM_MAP_PATH,
                    0.0,
                    DeadPixelQC_OFX::DEFAULT_MAP_PATH);
                if (!mapPath.empty()) {
                    runLoadMap = true;
                } else {
                    setMapStatus = true;
                    nextMapStatus = "Load failed: empty path";
                }
            }
        }

        if (runExport) {
            const bool useJson = (exportFormat == 1);
            const bool ok = useJson
                ? DeadPixelQC::ExportJSON::writeEvents(exportPath, exportSnapshot)
                : DeadPixelQC::ExportCSV::writeEvents(exportPath, exportSnapshot);

            std::ostringstream status;
            if (ok) {
                status << "Export OK: " << exportPath;
            } else {
                status << "Export failed: " << exportPath;
            }
            nextExportStatus = status.str();
            setExportStatus = true;
        }

        if (runSaveMap) {
            const bool ok = DeadPixelQC::ExportDefectMap::write(mapPath, mapSnapshot);
            setMapStatus = true;
            std::ostringstream status;
            if (ok) {
                status << "Saved: " << mapPath;
            } else {
                status << "Save failed: " << mapPath;
            }
            nextMapStatus = status.str();
        }

        if (runLoadMap) {
            const bool ok = DeadPixelQC::ImportDefectMap::read(mapPath, loadedMap);
            if (ok) {
                {
                    std::lock_guard<std::mutex> lock(instanceData->mutex);
                    instanceData->defectMap = loadedMap;
                }
                setMapSize = true;
                setMapStatus = true;
                nextMapSize = formatMapSize(loadedMap.size());
                std::ostringstream status;
                status << "Loaded: " << loadedMap.size() << " points";
                nextMapStatus = status.str();
            } else {
                setMapStatus = true;
                std::ostringstream status;
                status << "Load failed: " << mapPath;
                nextMapStatus = status.str();
            }
        }

        if (setCount || setIndex || setInfo || setExportStatus || setMapSize || setMapStatus) {
            instanceData->suppressInstanceChanged.store(true, std::memory_order_release);
            if (setCount) {
                setParamString(paramSet, DeadPixelQC_OFX::PARAM_EVENTS_COUNT, nextCount);
            }
            if (setIndex) {
                setParamInt(paramSet, DeadPixelQC_OFX::PARAM_SELECTED_EVENT_INDEX, nextIndex);
            }
            if (setInfo) {
                setParamString(paramSet, DeadPixelQC_OFX::PARAM_SELECTED_EVENT_INFO, nextInfo);
            }
            if (setExportStatus) {
                setParamString(paramSet, DeadPixelQC_OFX::PARAM_EXPORT_STATUS, nextExportStatus);
            }
            if (setMapSize) {
                setParamString(paramSet, DeadPixelQC_OFX::PARAM_MAP_SIZE, nextMapSize);
            }
            if (setMapStatus) {
                setParamString(paramSet, DeadPixelQC_OFX::PARAM_MAP_STATUS, nextMapStatus);
            }
            instanceData->suppressInstanceChanged.store(false, std::memory_order_release);
        }
    }

    return kOfxStatOK;
}
#endif

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
#if !DPQC_MINIMAL_UI
                const int workflowMode = getParamIntAtTime(paramSet, DeadPixelQC_OFX::PARAM_WORKFLOW_MODE, time, 0);
#else
                const int workflowMode = 0;
#endif
                const bool runDetection = (workflowMode != 2); // 2 = Map_Apply
                const bool effectEnabled = getParamBoolAtTime(paramSet, kParamEnable, time, true);

                OFXW::SpatialResult workerResult;
                if (runDetection) {
                    OFXW::SpatialParams workerParams;
                    workerParams.enable = effectEnabled;
                    workerParams.lumaThreshold = static_cast<float>(getParamDoubleAtTime(paramSet, kParamThreshold, time, 0.98));
                    workerParams.whitenessThreshold = 0.05f;
                    workerParams.robustZ = 10.0f;
                    workerParams.maxClusterArea = 4;
#if !DPQC_MINIMAL_UI
                    workerParams.outputMode = resolveSpatialOutputMode(paramSet, time);
#else
                    const int mode = getParamIntAtTime(paramSet, kParamMode, time, 0);
                    if (!hasParamSet || mode <= 0) {
                        workerParams.outputMode = OFXW::SpatialOutputMode::PassThrough;
                    } else if (mode == 1) {
                        workerParams.outputMode = OFXW::SpatialOutputMode::CandidatesOverlay;
                    } else {
                        workerParams.outputMode = OFXW::SpatialOutputMode::MaskOnly;
                    }
#endif
                    OFXW::SpatialWorker worker;
                    workerResult = worker.process(inView, outView, workerParams);
                    if (!workerResult.ok) {
                        copyImageRows(srcDesc, dstDesc);
                    }
                } else {
                    copyImageRows(srcDesc, dstDesc);
                }
                
#if !DPQC_MINIMAL_UI
                if (runDetection && workerResult.processed) {
                    if (workflowMode == 0 && instanceData->eventLogger) {
                        DeadPixelQC::EventLogger::Params loggerParams;
                        if (hasParam(paramSet, DeadPixelQC_OFX::PARAM_MAX_GAP_FRAMES)) {
                            loggerParams.maxGapFrames =
                                getParamIntAtTime(paramSet, DeadPixelQC_OFX::PARAM_MAX_GAP_FRAMES, time, loggerParams.maxGapFrames);
                        }
                        if (hasParam(paramSet, DeadPixelQC_OFX::PARAM_STUCK_WINDOW_FRAMES) &&
                            hasParam(paramSet, DeadPixelQC_OFX::PARAM_STUCK_MIN_FRACTION)) {
                            const int windowFrames =
                                getParamIntAtTime(paramSet, DeadPixelQC_OFX::PARAM_STUCK_WINDOW_FRAMES, time, DeadPixelQC_OFX::DEFAULT_STUCK_WINDOW_FRAMES);
                            const double minFraction =
                                getParamDoubleAtTime(paramSet, DeadPixelQC_OFX::PARAM_STUCK_MIN_FRACTION, time, DeadPixelQC_OFX::DEFAULT_STUCK_MIN_FRACTION);
                            const int derivedPersistence =
                                static_cast<int>(std::lround(std::max(0.0, static_cast<double>(windowFrames) * minFraction)));
                            if (derivedPersistence > 0) {
                                loggerParams.minPersistenceFrames = derivedPersistence;
                            }
                        }

                        const DeadPixelQC::DetectionHitList hits = toDetectionHits(workerResult.detection);

                        std::lock_guard<std::mutex> lock(instanceData->mutex);
                        instanceData->eventLogger->setParams(loggerParams);
                        instanceData->eventLogger->ingestFrame(
                            static_cast<DeadPixelQC::i32>(workerResult.detection.frameIndex >= 0 ? workerResult.detection.frameIndex : 0),
                            hits);
                    } else if (workflowMode == 1) {
                        const DeadPixelQC::DetectionHitList hits = toDetectionHits(workerResult.detection);
                        std::lock_guard<std::mutex> lock(instanceData->mutex);
                        instanceData->mapAccumulator.ingestFrame(hits, srcWidth, srcHeight);
                    }
                }

                if (workflowMode == 2 && effectEnabled) {
                    std::vector<DeadPixelQC::PixelCoord> mapPixels;
                    {
                        std::lock_guard<std::mutex> lock(instanceData->mutex);
                        mapPixels.reserve(instanceData->defectMap.points().size());
                        for (const auto& p : instanceData->defectMap.points()) {
                            DeadPixelQC::PixelCoord coord;
                            coord.x = mapNormalizedToPixel(p.u, dstWidth);
                            coord.y = mapNormalizedToPixel(p.v, dstHeight);
                            mapPixels.push_back(coord);
                        }
                    }

                    std::sort(mapPixels.begin(), mapPixels.end(), [](const DeadPixelQC::PixelCoord& a, const DeadPixelQC::PixelCoord& b) {
                        return (a.y < b.y) || (a.y == b.y && a.x < b.x);
                    });
                    mapPixels.erase(std::unique(mapPixels.begin(), mapPixels.end(), [](const DeadPixelQC::PixelCoord& a, const DeadPixelQC::PixelCoord& b) {
                        return a.x == b.x && a.y == b.y;
                    }), mapPixels.end());

                    if (!mapPixels.empty()) {
                        DeadPixelQC::RepairParams repairParams;
                        repairParams.enable = true;
                        repairParams.method = DeadPixelQC::RepairMethod::NeighborMedian;
                        if (hasParam(paramSet, DeadPixelQC_OFX::PARAM_REPAIR_METHOD)) {
                            const int method = getParamIntAtTime(paramSet, DeadPixelQC_OFX::PARAM_REPAIR_METHOD, time, 0);
                            repairParams.method = (method == 1)
                                ? DeadPixelQC::RepairMethod::DirectionalMedian
                                : DeadPixelQC::RepairMethod::NeighborMedian;
                        }

                        const DeadPixelQC::PixelFormat dstFormat = toCoreFormat(dstDesc.components, dstDesc.bitDepth);
                        DeadPixelQC::ImageBuffer dstBuffer(dstDesc.data, dstFormat, dstWidth, dstHeight, dstDesc.rowBytes);
                        DeadPixelQC::Fixer fixer(repairParams);
                        fixer.repairCoordinates(dstBuffer, mapPixels);
                    }
                }
#endif
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
#if !DPQC_MINIMAL_UI
    if (std::strcmp(action, kOfxActionInstanceChanged) == 0) {
        return instanceChanged(effect, inArgs);
    }
#endif

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
