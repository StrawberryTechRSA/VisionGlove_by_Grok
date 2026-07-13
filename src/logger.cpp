#include "visionglove/logger.hpp"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace vg {

Logger& Logger::instance() {
    static Logger inst;
    return inst;
}

void Logger::set_level(LogLevel level) {
    std::lock_guard lock(mu_);
    level_ = level;
}

void Logger::set_file(const std::string& path) {
    std::lock_guard lock(mu_);
    file_.close();
    file_.open(path, std::ios::app);
}

static const char* level_name(LogLevel l) {
    switch (l) {
        case LogLevel::Debug: return "DEBUG";
        case LogLevel::Info: return "INFO";
        case LogLevel::Warn: return "WARN";
        case LogLevel::Error: return "ERROR";
    }
    return "?";
}

void Logger::log(LogLevel level, std::string_view component, std::string_view message) {
    std::lock_guard lock(mu_);
    if (static_cast<int>(level) < static_cast<int>(level_)) return;

    using namespace std::chrono;
    const auto now = system_clock::now();
    const auto t = system_clock::to_time_t(now);
    std::tm tm{};
#if defined(_WIN32)
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S") << " [" << level_name(level) << "] "
        << component << ": " << message << '\n';
    const std::string line = oss.str();
    std::cout << line;
    if (file_.is_open()) {
        file_ << line;
        file_.flush();
    }
}

}  // namespace vg
