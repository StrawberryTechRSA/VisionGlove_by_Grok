#pragma once

#include <fstream>
#include <mutex>
#include <string>
#include <string_view>

namespace vg {

enum class LogLevel { Debug = 0, Info, Warn, Error };

class Logger {
public:
    static Logger& instance();

    void set_level(LogLevel level);
    void set_file(const std::string& path);
    void log(LogLevel level, std::string_view component, std::string_view message);

    void debug(std::string_view c, std::string_view m) { log(LogLevel::Debug, c, m); }
    void info(std::string_view c, std::string_view m) { log(LogLevel::Info, c, m); }
    void warn(std::string_view c, std::string_view m) { log(LogLevel::Warn, c, m); }
    void error(std::string_view c, std::string_view m) { log(LogLevel::Error, c, m); }

private:
    Logger() = default;
    std::mutex mu_;
    LogLevel level_ = LogLevel::Info;
    std::ofstream file_;
};

// Convenience macros
#define VG_LOG_DEBUG(comp, msg) ::vg::Logger::instance().debug(comp, msg)
#define VG_LOG_INFO(comp, msg)  ::vg::Logger::instance().info(comp, msg)
#define VG_LOG_WARN(comp, msg)  ::vg::Logger::instance().warn(comp, msg)
#define VG_LOG_ERROR(comp, msg) ::vg::Logger::instance().error(comp, msg)

}  // namespace vg
