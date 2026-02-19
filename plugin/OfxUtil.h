#pragma once

#include "ofxImageEffect.h"
#include "ofxMemory.h"
#include "ofxMultiThread.h"
#include <cstdint>
#include <string>
#include <vector>

// Include core headers
#include "../core/Types.h"
#include "../core/PixelFormatAdapter.h"

namespace DeadPixelQC_OFX {

// Forward declarations
extern const OfxImageEffectSuiteV1* gEffectSuite;
extern const OfxPropertySuiteV1* gPropertySuite;
extern const OfxParameterSuiteV1* gParamSuite;
extern const OfxMemorySuiteV1* gMemorySuite;
extern const OfxMultiThreadSuiteV1* gMultiThreadSuite;
extern const OfxMessageSuiteV1* gMessageSuite;

// OFX utility functions
class OfxUtil {
public:
    // Convert OFX pixel component type to our PixelFormat
    static DeadPixelQC::PixelFormat ofxToPixelFormat(const char* component,
                                                     int bitsPerComponent) {
        if (strcmp(component, kOfxImageComponentRGBA) == 0) {
            if (bitsPerComponent == 8) return DeadPixelQC::PixelFormat::RGBA8;
            if (bitsPerComponent == 16) return DeadPixelQC::PixelFormat::RGBA16;
            if (bitsPerComponent == 32) return DeadPixelQC::PixelFormat::RGBA32F;
        } else if (strcmp(component, kOfxImageComponentRGB) == 0) {
            if (bitsPerComponent == 8) return DeadPixelQC::PixelFormat::RGB8;
            if (bitsPerComponent == 16) return DeadPixelQC::PixelFormat::RGB16;
            if (bitsPerComponent == 32) return DeadPixelQC::PixelFormat::RGB32F;
        }
        return DeadPixelQC::PixelFormat::Unknown;
    }
    
    // Get image data from OFX image handle
    static DeadPixelQC::ImageBuffer getImageBuffer(OfxPropertySetHandle imageHandle) {
        void* data = nullptr;
        int rowBytes = 0;
        double par = 1.0;
        OfxRectI bounds = {0, 0, 0, 0};
        char* component = nullptr;
        int bitsPerComponent = 0;
        
        // Get image properties
        gPropertySuite->propGetPointer(imageHandle, kOfxImagePropData, 0, &data);
        gPropertySuite->propGetInt(imageHandle, kOfxImagePropRowBytes, 0, &rowBytes);
        gPropertySuite->propGetDouble(imageHandle, kOfxImagePropPixelAspectRatio, 0, &par);
        gPropertySuite->propGetIntN(imageHandle, kOfxImagePropBounds, 4, &bounds.x1);
        gPropertySuite->propGetString(imageHandle, kOfxImageEffectPropComponents, 0, &component);
        gPropertySuite->propGetInt(imageHandle, kOfxImageEffectPropPixelDepth, 0, &bitsPerComponent);
        
        DeadPixelQC::PixelFormat format = ofxToPixelFormat(component, bitsPerComponent);
        int width = bounds.x2 - bounds.x1;
        int height = bounds.y2 - bounds.y1;
        
        return DeadPixelQC::ImageBuffer(data, format, width, height, rowBytes);
    }
    
    // Note: Image memory locking is handled by the host through clipGetImage/clipReleaseImage
    // No need for explicit memory locking functions
    
    // Get parameter values
    static bool getParamDouble(OfxParamSetHandle paramSet, 
                               const char* paramName, 
                               double& value) {
        OfxParamHandle paramHandle = nullptr;
        if (gParamSuite->paramGetHandle(paramSet, paramName, &paramHandle, nullptr) != kOfxStatOK) {
            return false;
        }
        return gParamSuite->paramGetValue(paramHandle, &value) == kOfxStatOK;
    }
    
    static bool getParamDoubleAtTime(OfxParamSetHandle paramSet,
                                     const char* paramName,
                                     double time,
                                     double& value) {
        OfxParamHandle paramHandle = nullptr;
        if (gParamSuite->paramGetHandle(paramSet, paramName, &paramHandle, nullptr) != kOfxStatOK) {
            return false;
        }
        return gParamSuite->paramGetValueAtTime(paramHandle, time, &value) == kOfxStatOK;
    }
    
    static bool getParamInt(OfxParamSetHandle paramSet,
                            const char* paramName,
                            int& value) {
        double doubleValue = 0.0;
        if (!getParamDouble(paramSet, paramName, doubleValue)) {
            return false;
        }
        value = static_cast<int>(doubleValue);
        return true;
    }
    
    static bool getParamIntAtTime(OfxParamSetHandle paramSet,
                                  const char* paramName,
                                  double time,
                                  int& value) {
        double doubleValue = 0.0;
        if (!getParamDoubleAtTime(paramSet, paramName, time, doubleValue)) {
            return false;
        }
        value = static_cast<int>(doubleValue);
        return true;
    }
    
    static bool getParamBool(OfxParamSetHandle paramSet,
                             const char* paramName,
                             bool& value) {
        double doubleValue = 0.0;
        if (!getParamDouble(paramSet, paramName, doubleValue)) {
            return false;
        }
        value = (doubleValue != 0.0);
        return true;
    }
    
    static bool getParamBoolAtTime(OfxParamSetHandle paramSet,
                                   const char* paramName,
                                   double time,
                                   bool& value) {
        double doubleValue = 0.0;
        if (!getParamDoubleAtTime(paramSet, paramName, time, doubleValue)) {
            return false;
        }
        value = (doubleValue != 0.0);
        return true;
    }
    
    static bool getParamString(OfxParamSetHandle paramSet,
                               const char* paramName,
                               std::string& value) {
        OfxParamHandle paramHandle = nullptr;
        if (gParamSuite->paramGetHandle(paramSet, paramName, &paramHandle, nullptr) != kOfxStatOK) {
            return false;
        }
        
        const char* strValue = nullptr;
        if (gParamSuite->paramGetValue(paramHandle, &strValue) != kOfxStatOK) {
            return false;
        }
        
        if (strValue) {
            value = strValue;
        } else {
            value.clear();
        }
        return true;
    }
    
    // Set parameter values
    static bool setParamDouble(OfxParamSetHandle paramSet,
                               const char* paramName,
                               double value) {
        OfxParamHandle paramHandle = nullptr;
        if (gParamSuite->paramGetHandle(paramSet, paramName, &paramHandle, nullptr) != kOfxStatOK) {
            return false;
        }
        return gParamSuite->paramSetValue(paramHandle, value) == kOfxStatOK;
    }
    
    // Error reporting
    static void reportError(OfxImageEffectHandle instance,
                            const std::string& message) {
        gMessageSuite->message(instance, kOfxMessageError, nullptr, message.c_str());
    }
    
    static void reportWarning(OfxImageEffectHandle instance,
                              const std::string& message) {
        gMessageSuite->message(instance, kOfxMessageWarning, nullptr, message.c_str());
    }
    
    static void reportMessage(OfxImageEffectHandle instance,
                              const std::string& message) {
        gMessageSuite->message(instance, kOfxMessageMessage, nullptr, message.c_str());
    }
    
    // Check if clip is connected
    static bool isClipConnected(OfxImageEffectHandle instance,
                                const char* clipName) {
        OfxImageClipHandle clipHandle = nullptr;
        if (gEffectSuite->clipGetHandle(instance, clipName, &clipHandle, nullptr) != kOfxStatOK) {
            return false;
        }
        
        OfxPropertySetHandle clipProps = nullptr;
        gEffectSuite->clipGetPropertySet(clipHandle, &clipProps);
        
        int connected = 0;
        gPropertySuite->propGetInt(clipProps, kOfxImageClipPropConnected, 0, &connected);
        return connected != 0;
    }
    
    // Get clip handle
    static OfxImageClipHandle getClipHandle(OfxImageEffectHandle instance,
                                            const char* clipName) {
        OfxImageClipHandle clipHandle = nullptr;
        gEffectSuite->clipGetHandle(instance, clipName, &clipHandle, nullptr);
        return clipHandle;
    }
};

} // namespace DeadPixelQC_OFX