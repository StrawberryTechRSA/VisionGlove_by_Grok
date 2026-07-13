#pragma once

#include <array>
#include <string>
#include <vector>

namespace vg {

struct SystemConfig {
    std::string name = "VisionGlove";
    std::string version = "2.0.0-grok";
    bool debug_mode = false;
    std::string log_level = "INFO";
};

struct SensorsConfig {
    bool enabled = true;
    int sample_rate = 100;           // Hz
    bool calibration_required = true;
    double timeout_s = 5.0;
    double unusual_accel_threshold = 15.0;  // m/s^2
    double closed_threshold = 0.70;
    double open_threshold = 0.30;
    double panic_window_s = 2.0;
    int panic_fist_count = 3;
};

struct VisionConfig {
    bool enabled = true;
    int camera_index = 0;
    std::array<int, 2> resolution{{640, 480}};
    int fps = 30;
    double detection_threshold = 0.7;
    int person_threshold = 3;
    bool simulate = true;            // no camera required
};

struct HapticsConfig {
    bool enabled = true;
    double intensity = 0.8;
    double duration_s = 1.0;
    std::string pattern = "pulse";
};

struct SmsConfig {
    std::string provider = "twilio";
    std::string account_sid;
    std::string auth_token;
    std::string from_number;
};

struct CommunicationsConfig {
    std::string emergency_contact;
    std::string police_number;
    SmsConfig sms;
    bool dry_run = true;             // never send real SMS unless explicitly disabled
};

struct SecurityConfig {
    bool encryption_enabled = true;
    int key_rotation_interval_s = 3600;
    int max_failed_attempts = 3;
};

struct LivestreamConfig {
    bool enabled = true;
    std::string quality = "medium";
    std::string platform = "youtube";
    std::string stream_key;
    int max_duration_s = 3600;
    bool dry_run = true;
};

struct AppConfig {
    SystemConfig system;
    SensorsConfig sensors;
    VisionConfig vision;
    HapticsConfig haptics;
    CommunicationsConfig communications;
    SecurityConfig security;
    LivestreamConfig livestream;
};

// Load JSON config; on failure returns defaults and sets ok=false.
// Supports the same schema as Claude's config.json plus Grok extensions.
[[nodiscard]] AppConfig load_config(const std::string& path, bool* ok = nullptr);
[[nodiscard]] bool save_config(const std::string& path, const AppConfig& cfg);
[[nodiscard]] bool validate_config(const AppConfig& cfg, std::string* error = nullptr);

}  // namespace vg
