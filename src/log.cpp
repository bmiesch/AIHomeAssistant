#include <ctime>
#include <mutex>
#include "log.h"

std::mutex LogMutex;

static std::string LogLevelString(LogLevel lvl) {
    switch (lvl) {
        case LogLevel::INFO: return "INFO";
        case LogLevel::DEBUG: return "DEBUG";
        case LogLevel::WARN: return "WARN";
        case LogLevel::ERROR: return "ERROR";
        default: return "Unknown";
    }
}

static std::string CurrentDateTime() {
    std::time_t now = std::time(nullptr);
    char buf[100];
    if (std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::localtime(&now))) {
        return buf;
    }
    return "unknown time";
}

void log(LogLevel lvl, const std::string& msg) {
    std::lock_guard<std::mutex> lock(LogMutex);
    std::cout << CurrentDateTime() << " [" << LogLevelString(lvl) << "] " << msg << std::endl;
}