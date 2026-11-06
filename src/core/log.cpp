// PS4 HEN ULTIMATE - core/log.cpp
#include "core/log.h"

#include <cstdio>

namespace henu {

void Logger::attachFile(IFileSystem* fs, const std::string& dir) {
    fs_ = fs;
    path_ = dir + "/app.log";
    fileSize_ = 0;
    if (fs_ && fs_->exists(path_)) {
        auto existing = fs_->readFile(path_, kMaxFile * 2);
        if (existing) {
            pending_ = existing.value();
            fileSize_ = pending_.size();
        }
    }
}

void Logger::log(LogLevel lvl, const std::string& msg) {
    static const char* names[] = {"ERROR", "WARN", "INFO", "DEBUG"};
    std::string line = std::string("[") + names[static_cast<int>(lvl)] + "] " + msg;
    std::fprintf(stderr, "%s\n", line.c_str());
    recent_.push_back(line);
    if (recent_.size() > kMaxRecent) recent_.pop_front();
    if (fs_) {
        pending_ += line + "\n";
        fileSize_ += line.size() + 1;
        if (lvl == LogLevel::Error) flush();
    }
}

void Logger::rotate() {
    if (!fs_) return;
    for (int i = 4; i >= 1; --i) {
        std::string from = i == 1 ? path_ : path_ + "." + std::to_string(i - 1);
        std::string to = path_ + "." + std::to_string(i);
        if (fs_->exists(from)) {
            if (fs_->exists(to)) fs_->remove(to);
            fs_->rename(from, to);
        }
    }
    pending_.clear();
    fileSize_ = 0;
}

void Logger::flush() {
    if (!fs_) return;
    // The current file is small (<= ~1 MB) by design, so it is rewritten whole.
    fs_->writeFile(path_, pending_);
    if (fileSize_ > kMaxFile) rotate();
}

}  // namespace henu
