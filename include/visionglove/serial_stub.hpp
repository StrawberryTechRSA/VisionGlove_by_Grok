#pragma once
// Serial / file sensor feed stub — same Stage B path as real hardware later.
// Protocol (one packet per line, # comments allowed):
//   FLEX,thumb,index,middle,ring,pinky     values 0.0 .. 1.0  (0=open, 1=bent)
//   IMU,ax,ay,az,gx,gy,gz                  m/s^2 and rad/s
//   VG,t,i,m,r,p                           alias for FLEX

#include "visionglove/types.hpp"

#include <array>
#include <fstream>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace vg {

struct SerialPacket {
    bool has_flex = false;
    std::array<double, 5> flex{{0, 0, 0, 0, 0}};
    bool has_imu = false;
    Vec3 accel{};
    Vec3 gyro{};
};

// Pure parse — no I/O (unit-testable)
[[nodiscard]] bool parse_sensor_line(const std::string& line, SerialPacket& out);

class SerialStub {
public:
    // Open a text file of packets. If loop=true, rewind at EOF (demo feed).
    bool open_file(const std::string& path, bool loop = true);
    void close();

    // Apply next line from file into last_ (for streaming demos)
    bool pump_next();

    // Inject a single line (tests / live serial reader later)
    bool apply_line(const std::string& line);

    [[nodiscard]] bool has_data() const;
    [[nodiscard]] SerialPacket latest() const;
    [[nodiscard]] std::size_t lines_consumed() const { return lines_consumed_; }
    [[nodiscard]] const std::string& path() const { return path_; }

private:
    mutable std::mutex mu_;
    std::ifstream file_;
    std::string path_;
    bool loop_ = true;
    bool open_ = false;
    SerialPacket last_{};
    bool has_ = false;
    std::size_t lines_consumed_ = 0;
};

}  // namespace vg
