#ifndef DEADPIXELQC_DEBUGLOG_H
#define DEADPIXELQC_DEBUGLOG_H

#include <string>
#include <fstream>
#include <mutex>

namespace DeadPixelQC_OFX {

class DebugLog {
public:
    static DebugLog& instance() {
        static DebugLog log;
        return log;
    }
    
    void log(const std::string& message) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!file_.is_open()) {
            file_.open("C:\\DeadPixelQC_OFX\\plugin_debug.log", std::ios::app);
        }
        if (file_.is_open()) {
            file_ << message << std::endl;
            file_.flush();
        }
    }
    
    void logAction(const std::string& action, const std::string& handle = "") {
        std::string msg = "[OFX] Action: " + action;
        if (!handle.empty()) {
            msg += ", Handle: " + handle;
        }
        log(msg);
    }
    
    void logError(const std::string& error) {
        log("[ERROR] " + error);
    }
    
    void logWarning(const std::string& warning) {
        log("[WARNING] " + warning);
    }
    
private:
    DebugLog() = default;
    ~DebugLog() {
        if (file_.is_open()) {
            file_.close();
        }
    }
    
    std::ofstream file_;
    std::mutex mutex_;
};

// Макросы для удобства
#define DEBUG_LOG(msg) DeadPixelQC_OFX::DebugLog::instance().log(msg)
#define DEBUG_LOG_ACTION(action, handle) DeadPixelQC_OFX::DebugLog::instance().logAction(action, handle)
#define DEBUG_LOG_ERROR(error) DeadPixelQC_OFX::DebugLog::instance().logError(error)
#define DEBUG_LOG_WARNING(warning) DeadPixelQC_OFX::DebugLog::instance().logWarning(warning)

} // namespace DeadPixelQC_OFX

#endif // DEADPIXELQC_DEBUGLOG_H
