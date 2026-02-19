#ifndef DEADPIXELQC_DEBUGLOG_H
#define DEADPIXELQC_DEBUGLOG_H

#include <string>
#include <fstream>
#include <mutex>
#include <cstdlib>

#if defined(_WIN32)
#include <windows.h>
#endif

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
            openIfNeededNoThrow();
        }
        if (file_.is_open()) {
            try {
                file_ << message << std::endl;
                file_.flush();
            } catch (...) {
                // Silent fail: logging must never crash plugin loading/rendering.
            }
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

    static std::string getLogPath() {
#if defined(_WIN32)
        char tempPath[MAX_PATH] = {0};
        const DWORD len = GetTempPathA(MAX_PATH, tempPath);
        if (len > 0 && len < MAX_PATH) {
            std::string path(tempPath);
            if (!path.empty() && path.back() != '\\' && path.back() != '/') {
                path += "\\";
            }
            path += "deadpixelqc_debug.log";
            return path;
        }
#endif
        return "C:\\temp\\deadpixelqc_debug.log";
    }

    void openIfNeededNoThrow() {
        if (file_.is_open()) {
            return;
        }
        try {
            file_.open(getLogPath(), std::ios::app);
        } catch (...) {
            // Silent fail: logging is optional.
        }
    }
};

// Макросы для удобства
#define DEBUG_LOG(msg) DeadPixelQC_OFX::DebugLog::instance().log(msg)
#define DEBUG_LOG_ACTION(action, handle) DeadPixelQC_OFX::DebugLog::instance().logAction(action, handle)
#define DEBUG_LOG_ERROR(error) DeadPixelQC_OFX::DebugLog::instance().logError(error)
#define DEBUG_LOG_WARNING(warning) DeadPixelQC_OFX::DebugLog::instance().logWarning(warning)

} // namespace DeadPixelQC_OFX

#endif // DEADPIXELQC_DEBUGLOG_H
