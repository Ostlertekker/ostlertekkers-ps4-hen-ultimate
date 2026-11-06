// PS4 HEN ULTIMATE - core/log.h
// Simple logger: memory ring (for the diagnostics screen) + optional file
// sink with size-based rotation (app.log -> app.log.1 ... .4).
#pragma once

#include <deque>
#include <string>

#include "platform/platform.h"

namespace henu {

enum class LogLevel { Error, Warn, Info, Debug };

class Logger {
public:
    void attachFile(IFileSystem* fs, const std::string& dir);   // dir must exist
    void log(LogLevel lvl, const std::string& msg);
    void error(const std::string& m) { log(LogLevel::Error, m); }
    void warn(const std::string& m) { log(LogLevel::Warn, m); }
    void info(const std::string& m) { log(LogLevel::Info, m); }
    const std::deque<std::string>& recent() const { return recent_; }
    void flush();

private:
    void rotate();
    IFileSystem* fs_ = nullptr;
    std::string path_;
    std::string pending_;
    size_t fileSize_ = 0;
    std::deque<std::string> recent_;
    static constexpr size_t kMaxRecent = 200;
    static constexpr size_t kMaxFile = 1024 * 1024;
};

}  // namespace henu
