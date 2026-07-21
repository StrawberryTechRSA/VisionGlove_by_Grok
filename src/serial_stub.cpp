#include "visionglove/serial_stub.hpp"
#include "visionglove/logger.hpp"

#include <cmath>
#include <sstream>

namespace vg {
namespace {

double clamp01(double v) {
    if (v < 0.0) return 0.0;
    if (v > 1.0) return 1.0;
    return v;
}

std::string trim(std::string s) {
    while (!s.empty() && (s.back() == '\r' || s.back() == '\n' || s.back() == ' ' || s.back() == '\t'))
        s.pop_back();
    std::size_t i = 0;
    while (i < s.size() && (s[i] == ' ' || s[i] == '\t')) ++i;
    return s.substr(i);
}

}  // namespace

bool parse_sensor_line(const std::string& raw, SerialPacket& out) {
    std::string line = trim(raw);
    if (line.empty() || line[0] == '#') return false;

    std::vector<std::string> parts;
    std::stringstream ss(line);
    std::string item;
    while (std::getline(ss, item, ',')) {
        parts.push_back(trim(item));
    }
    if (parts.empty()) return false;

    const std::string& tag = parts[0];
    try {
        if ((tag == "FLEX" || tag == "VG") && parts.size() >= 6) {
            out.has_flex = true;
            for (int i = 0; i < 5; ++i)
                out.flex[static_cast<std::size_t>(i)] = clamp01(std::stod(parts[static_cast<std::size_t>(i + 1)]));
            return true;
        }
        if (tag == "IMU" && parts.size() >= 7) {
            out.has_imu = true;
            out.accel = {std::stod(parts[1]), std::stod(parts[2]), std::stod(parts[3])};
            out.gyro = {std::stod(parts[4]), std::stod(parts[5]), std::stod(parts[6])};
            return true;
        }
    } catch (...) {
        return false;
    }
    return false;
}

bool SerialStub::open_file(const std::string& path, bool loop) {
    std::lock_guard lock(mu_);
    file_.close();
    file_.open(path);
    if (!file_) {
        open_ = false;
        path_.clear();
        VG_LOG_ERROR("SerialStub", "Cannot open feed file: " + path);
        return false;
    }
    path_ = path;
    loop_ = loop;
    open_ = true;
    lines_consumed_ = 0;
    VG_LOG_INFO("SerialStub", "Opened sensor feed: " + path + (loop ? " (loop)" : ""));
    return true;
}

void SerialStub::close() {
    std::lock_guard lock(mu_);
    file_.close();
    open_ = false;
}

bool SerialStub::apply_line(const std::string& line) {
    SerialPacket pkt;
    // Merge into last_ so FLEX then IMU can stack
    SerialPacket merged;
    {
        std::lock_guard lock(mu_);
        merged = last_;
    }
    if (!parse_sensor_line(line, pkt)) return false;
    if (pkt.has_flex) {
        merged.has_flex = true;
        merged.flex = pkt.flex;
    }
    if (pkt.has_imu) {
        merged.has_imu = true;
        merged.accel = pkt.accel;
        merged.gyro = pkt.gyro;
    }
    std::lock_guard lock(mu_);
    last_ = merged;
    has_ = merged.has_flex || merged.has_imu;
    ++lines_consumed_;
    return true;
}

bool SerialStub::pump_next() {
    std::string line;
    {
        std::lock_guard lock(mu_);
        if (!open_ || !file_.is_open()) return false;
        while (true) {
            if (!std::getline(file_, line)) {
                if (!loop_) return false;
                file_.clear();
                file_.seekg(0);
                if (!std::getline(file_, line)) return false;
            }
            // skip blanks/comments without counting as consume failure
            std::string t = trim(line);
            if (t.empty() || t[0] == '#') continue;
            break;
        }
    }
    return apply_line(line);
}

bool SerialStub::has_data() const {
    std::lock_guard lock(mu_);
    return has_;
}

SerialPacket SerialStub::latest() const {
    std::lock_guard lock(mu_);
    return last_;
}

}  // namespace vg
