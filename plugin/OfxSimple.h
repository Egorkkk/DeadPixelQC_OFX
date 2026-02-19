#pragma once

// Simplified OpenFX definitions for compilation
// In a real plugin, use the actual OpenFX SDK headers

#include <cstddef>
#include <string>
#include <vector>
#include <mutex>

namespace DeadPixelQC {

// Basic OpenFX types
typedef int OfxStatus;
typedef void* OfxPropertySetHandle;
typedef void* OfxImageEffectHandle;
typedef void* OfxParamHandle;
typedef void* OfxImageClipHandle;
typedef double OfxTime;

// Status codes
const OfxStatus kOfxStatOK = 0;
const OfxStatus kOfxStatFailed = 1;
const OfxStatus kOfxStatErrUnknown = 2;
const OfxStatus kOfxStatErrMemory = 3;

// Property names
const char* kOfxPropInstanceData = "OfxPropInstanceData";
const char* kOfxImagePropData = "OfxImagePropData";
const char* kOfxImagePropBounds = "OfxImagePropBounds";
const char* kOfxImagePropPixelFormat = "OfxImagePropPixelFormat";
const char* kOfxImageComponentRGBA = "RGBA";
const char* kOfxImageComponentRGB = "RGB";
const char* kOfxImageComponentAlpha = "Alpha";

// Message types
const int kOfxMessageError = 0;
const int kOfxMessageWarning = 1;
const int kOfxMessageMessage = 2;

// Simplified suite structures
struct OfxPropertySuiteV1 {
    OfxStatus (*propGetString)(OfxPropertySetHandle properties, const char* property, int index, char** value);
    OfxStatus (*propGetInt)(OfxPropertySetHandle properties, const char* property, int index, int* value);
    OfxStatus (*propGetDouble)(OfxPropertySetHandle properties, const char* property, int index, double* value);
    OfxStatus (*propGetPointer)(OfxPropertySetHandle properties, const char* property, int index, void** value);
    OfxStatus (*propSetPointer)(OfxPropertySetHandle properties, const char* property, int index, void* value);
};

struct OfxImageEffectSuiteV1 {
    OfxStatus (*clipGetImage)(OfxImageEffectHandle instance, const char* name, OfxTime time, const void* region, OfxPropertySetHandle* imageHandle);
};

struct OfxParamSuiteV1 {
    OfxStatus (*paramGetValue)(OfxParamHandle param, double* value);
};

struct OfxMessageSuiteV1 {
    OfxStatus (*message)(OfxImageEffectHandle instance, int type, const char* id, const char* format, ...);
};

struct OfxMemorySuiteV1 {
    void* (*alloc)(size_t nBytes);
    OfxStatus (*free)(void* ptr);
};

// Plugin instance data
struct PluginInstanceData {
    std::mutex mutex;
    void* userData;
    
    PluginInstanceData() : userData(nullptr) {}
};

// Utility functions
inline std::string getStringProperty(const OfxPropertySuiteV1* propSuite,
                                     const OfxPropertySetHandle properties,
                                     const char* property, int index) {
    char* value = nullptr;
    if (propSuite && propSuite->propGetString) {
        propSuite->propGetString(properties, property, index, &value);
    }
    return value ? std::string(value) : std::string();
}

inline int getIntProperty(const OfxPropertySuiteV1* propSuite,
                          const OfxPropertySetHandle properties,
                          const char* property, int index, int defaultValue = 0) {
    int value = defaultValue;
    if (propSuite && propSuite->propGetInt) {
        propSuite->propGetInt(properties, property, index, &value);
    }
    return value;
}

inline double getDoubleProperty(const OfxPropertySuiteV1* propSuite,
                                const OfxPropertySetHandle properties,
                                const char* property, int index, double defaultValue = 0.0) {
    double value = defaultValue;
    if (propSuite && propSuite->propGetDouble) {
        propSuite->propGetDouble(properties, property, index, &value);
    }
    return value;
}

inline void* getPointerProperty(const OfxPropertySuiteV1* propSuite,
                                const OfxPropertySetHandle properties,
                                const char* property, int index) {
    void* value = nullptr;
    if (propSuite && propSuite->propGetPointer) {
        propSuite->propGetPointer(properties, property, index, &value);
    }
    return value;
}

} // namespace DeadPixelQC