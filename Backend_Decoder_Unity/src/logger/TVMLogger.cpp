#include "TVMLogger.h"
#include <iostream>
#include <mutex>

#ifdef __ANDROID__
#include <android/log.h>
#endif

namespace TVMLogger {

// --- Safe function-local statics to avoid destruction race on shutdown ---

namespace {

std::mutex& GetLoggerMutex() {
    static std::mutex m;
    return m;
}

std::function<void(LogLevel, const std::string&)>& GetCurrentLogger() {
    static std::function<void(LogLevel, const std::string&)> f = nullptr;
    return f;
}

bool& GetLoggingEnabled() {
    static bool enabled = true;
    return enabled;
}

} // anonymous namespace

//Logging interface

void SetLogger(std::function<void(LogLevel, const std::string&)> callback) {
    std::lock_guard<std::mutex> lock(GetLoggerMutex());
    GetCurrentLogger() = std::move(callback);
}

void EnableLogging(bool enabled) {
    std::lock_guard<std::mutex> lock(GetLoggerMutex());
    GetLoggingEnabled() = enabled;
}

bool IsLoggingEnabled() {
    std::lock_guard<std::mutex> lock(GetLoggerMutex());
    return GetLoggingEnabled();
}

void Log(LogLevel level, const std::string& msg) {
    std::function<void(LogLevel, const std::string&)> loggerCopy;
    bool loggingEnabled = false;

    {
        std::lock_guard<std::mutex> lock(GetLoggerMutex());
        loggerCopy = GetCurrentLogger();
        loggingEnabled = GetLoggingEnabled();
    }

    // Suppress non-error logs if disabled
    if (!loggingEnabled && level != LogLevel::Error)
        return;

    // Avoid logging during stack unwinding (optional, C++17+)
#if __cplusplus >= 201703L
    if (std::uncaught_exceptions() > 0) return;
#endif

    const std::string fullMsg = "TVMDecoder Backend: " + msg;

    if (loggerCopy) {
        try {
            loggerCopy(level, fullMsg);
        } catch (const std::exception& e) {
            std::cerr << "[TVMLogger] Logger callback threw exception: " << e.what() << std::endl;
        } catch (...) {
            std::cerr << "[TVMLogger] Logger callback threw unknown exception." << std::endl;
        }
        return;
    }

#ifdef __ANDROID__
    int priority = ANDROID_LOG_INFO;
    if (level == LogLevel::Warning) priority = ANDROID_LOG_WARN;
    else if (level == LogLevel::Error) priority = ANDROID_LOG_ERROR;
    __android_log_print(priority, "TVMDecoder", "%s", fullMsg.c_str());
#else
    const char* prefix = "[INFO] ";
    if (level == LogLevel::Warning) prefix = "[WARN] ";
    else if (level == LogLevel::Error) prefix = "[ERROR] ";
    std::ostream& stream = (level == LogLevel::Error) ? std::cerr : std::cout;
    stream << prefix << fullMsg << std::endl;
#endif
}

} // namespace TVMLogger
