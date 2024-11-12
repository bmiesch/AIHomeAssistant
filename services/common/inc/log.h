#pragma once

#include <iostream>

enum class LogLevel {
    INFO,
    DEBUG,
    WARN,
    ERROR
};

void log(LogLevel lvl, const std::string& msg, const char* file, int line);

#define INFO_LOG(msg) log(LogLevel::INFO, msg, __FILE__, __LINE__)
#define DEBUG_LOG(msg) log(LogLevel::DEBUG, msg, __FILE__, __LINE__)
#define WARN_LOG(msg) log(LogLevel::WARN, msg, __FILE__, __LINE__)
#define ERROR_LOG(msg) log(LogLevel::ERROR, msg, __FILE__, __LINE__)
