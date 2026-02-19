#include "OfxUtil.h"

namespace DeadPixelQC_OFX {

// Define global suite pointers
const OfxImageEffectSuiteV1* gEffectSuite = nullptr;
const OfxPropertySuiteV1* gPropertySuite = nullptr;
const OfxParameterSuiteV1* gParamSuite = nullptr;
const OfxMemorySuiteV1* gMemorySuite = nullptr;
const OfxMultiThreadSuiteV1* gMultiThreadSuite = nullptr;
const OfxMessageSuiteV1* gMessageSuite = nullptr;

} // namespace DeadPixelQC_OFX