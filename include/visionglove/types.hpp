#pragma once
// VisionGlove by Grok — core types (C++20)
// Deterministic, header-only POD/value types for the real-time pipeline.

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace vg {

using Clock = std::chrono::steady_clock;
using TimePoint = Clock::time_point;
using Duration = Clock::duration;

// ---------------------------------------------------------------------------
// Math
// ---------------------------------------------------------------------------
struct Vec3 {
    double x = 0, y = 0, z = 0;

    constexpr Vec3() = default;
    constexpr Vec3(double x_, double y_, double z_) : x(x_), y(y_), z(z_) {}

    constexpr Vec3 operator+(const Vec3& o) const { return {x + o.x, y + o.y, z + o.z}; }
    constexpr Vec3 operator-(const Vec3& o) const { return {x - o.x, y - o.y, z - o.z}; }
    constexpr Vec3 operator*(double s) const { return {x * s, y * s, z * s}; }
    constexpr Vec3 operator/(double s) const { return {x / s, y / s, z / s}; }
    Vec3& operator+=(const Vec3& o) { x += o.x; y += o.y; z += o.z; return *this; }

    [[nodiscard]] constexpr double dot(const Vec3& o) const { return x * o.x + y * o.y + z * o.z; }
    [[nodiscard]] double norm() const { return std::sqrt(dot(*this)); }
    [[nodiscard]] Vec3 normalized() const {
        const double n = norm();
        return n > 1e-12 ? (*this) / n : Vec3{0, 0, 0};
    }
    [[nodiscard]] Vec3 cross(const Vec3& o) const {
        return {y * o.z - z * o.y, z * o.x - x * o.z, x * o.y - y * o.x};
    }
};

// Unit quaternion: w + xi + yj + zk
struct Quat {
    double w = 1, x = 0, y = 0, z = 0;

    [[nodiscard]] Quat normalized() const {
        const double n = std::sqrt(w * w + x * x + y * y + z * z);
        if (n < 1e-12) return {};
        return {w / n, x / n, y / n, z / n};
    }

    // Hamilton product
    [[nodiscard]] Quat operator*(const Quat& o) const {
        return {
            w * o.w - x * o.x - y * o.y - z * o.z,
            w * o.x + x * o.w + y * o.z - z * o.y,
            w * o.y - x * o.z + y * o.w + z * o.x,
            w * o.z + x * o.y - y * o.x + z * o.w
        };
    }

    // Integrate angular velocity (rad/s) over dt seconds
    [[nodiscard]] Quat integrate_gyro(const Vec3& gyro_rad_s, double dt) const {
        const Quat q_dot{
            0.5 * (-x * gyro_rad_s.x - y * gyro_rad_s.y - z * gyro_rad_s.z),
            0.5 * ( w * gyro_rad_s.x + y * gyro_rad_s.z - z * gyro_rad_s.y),
            0.5 * ( w * gyro_rad_s.y - x * gyro_rad_s.z + z * gyro_rad_s.x),
            0.5 * ( w * gyro_rad_s.z + x * gyro_rad_s.y - y * gyro_rad_s.x)
        };
        return Quat{w + q_dot.w * dt, x + q_dot.x * dt, y + q_dot.y * dt, z + q_dot.z * dt}.normalized();
    }

    // Roll, pitch, yaw (degrees)
    [[nodiscard]] Vec3 to_euler_deg() const {
        // roll (x)
        const double sinr = 2 * (w * x + y * z);
        const double cosr = 1 - 2 * (x * x + y * y);
        const double roll = std::atan2(sinr, cosr);
        // pitch (y)
        double sinp = 2 * (w * y - z * x);
        sinp = std::clamp(sinp, -1.0, 1.0);
        const double pitch = std::asin(sinp);
        // yaw (z)
        const double siny = 2 * (w * z + x * y);
        const double cosy = 1 - 2 * (y * y + z * z);
        const double yaw = std::atan2(siny, cosy);
        constexpr double R2D = 180.0 / 3.14159265358979323846;
        return {roll * R2D, pitch * R2D, yaw * R2D};
    }
};

// ---------------------------------------------------------------------------
// Domain enums
// ---------------------------------------------------------------------------
enum class ThreatLevel : std::uint8_t {
    Safe = 0,
    Caution = 1,
    Alert = 2,
    Emergency = 3
};

[[nodiscard]] inline std::string_view to_string(ThreatLevel t) {
    switch (t) {
        case ThreatLevel::Safe: return "Safe";
        case ThreatLevel::Caution: return "Caution";
        case ThreatLevel::Alert: return "Alert";
        case ThreatLevel::Emergency: return "Emergency";
    }
    return "Unknown";
}

enum class Finger : std::uint8_t { Thumb = 0, Index, Middle, Ring, Pinky, Count };

[[nodiscard]] inline std::string_view finger_name(Finger f) {
    switch (f) {
        case Finger::Thumb: return "thumb";
        case Finger::Index: return "index";
        case Finger::Middle: return "middle";
        case Finger::Ring: return "ring";
        case Finger::Pinky: return "pinky";
        default: return "unknown";
    }
}

enum class Gesture : std::uint8_t {
    None = 0,
    OpenHand,
    Fist,
    Pointing,
    Peace,
    ThumbsUp,
    Rock,            // index + pinky open ("horns") — beginner exercise #2
    PanicSequence    // triple-fist within window — emergency
};

[[nodiscard]] inline std::string_view to_string(Gesture g) {
    switch (g) {
        case Gesture::None: return "none";
        case Gesture::OpenHand: return "open_hand";
        case Gesture::Fist: return "fist";
        case Gesture::Pointing: return "pointing";
        case Gesture::Peace: return "peace_sign";
        case Gesture::ThumbsUp: return "thumbs_up";
        case Gesture::Rock: return "rock";
        case Gesture::PanicSequence: return "panic_sequence";
    }
    return "unknown";
}

// ---------------------------------------------------------------------------
// Sensor / vision snapshots (plain data for lock-free handoff)
// ---------------------------------------------------------------------------
struct FlexReading {
    double raw = 0;          // 0..1 uncalibrated
    double value = 0;        // 0..1 calibrated (0 extended, 1 fully bent)
    bool ok = true;
};

struct ImuReading {
    Vec3 accel{};            // m/s^2
    Vec3 gyro{};             // rad/s
    Vec3 mag{};              // arbitrary units
    Vec3 euler_deg{};        // roll, pitch, yaw
    Quat orientation{};
    double accel_mag = 0;
    double gyro_mag = 0;
    bool ok = true;
};

struct PressureReading {
    double raw = 0;
    double value = 0;        // 0..1
    bool ok = true;
};

struct SensorSnapshot {
    TimePoint timestamp{};
    std::array<FlexReading, 5> flex{};
    ImuReading imu{};
    std::array<PressureReading, 5> pressure{};

    // Processed
    double hand_closure = 0;
    double grip_strength = 0;
    double movement_magnitude = 0;
    bool unusual_movement = false;
    bool emergency_gesture = false;
    Gesture primary_gesture = Gesture::None;
    std::vector<Gesture> gestures;
};

struct PersonDetection {
    float x = 0, y = 0, w = 0, h = 0; // normalized bbox
    float confidence = 0;
};

struct VisionSnapshot {
    TimePoint timestamp{};
    std::uint64_t frame_id = 0;
    int person_count = 0;
    std::vector<PersonDetection> persons;
    double threat_score = 0;   // continuous 0..1 before level mapping
    ThreatLevel vision_threat = ThreatLevel::Safe;
    std::string description;
    bool ok = true;
};

struct SystemStatus {
    bool running = false;
    double uptime_s = 0;
    ThreatLevel threat = ThreatLevel::Safe;
    bool sensors_ok = false;
    bool vision_ok = false;
    bool haptics_ok = false;
    bool emergency_ok = false;
    double sensor_hz = 0;
    double vision_fps = 0;
    std::uint64_t cycles = 0;
};

struct EmergencyEvent {
    std::string id;
    TimePoint timestamp{};
    ThreatLevel level = ThreatLevel::Safe;
    std::string location = "Unknown";
    std::vector<std::string> actions;
    bool active = true;
};

}  // namespace vg
