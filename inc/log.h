#pragma once

#include <iostream>

enum class LogLevel {
    INFO,
    DEBUG,
    WARN,
    ERROR
};

void log(LogLevel lvl, const std::string& msg);

inline void INFO_LOG(const std::string&msg) {log(LogLevel::INFO, msg);}
inline void DEBUG_LOG(const std::string&msg) {log(LogLevel::DEBUG, msg);}
inline void WARN_LOG(const std::string&msg) {log(LogLevel::WARN, msg);}
inline void ERROR_LOG(const std::string&msg) {log(LogLevel::ERROR, msg);}

