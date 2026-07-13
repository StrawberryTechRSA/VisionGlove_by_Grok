#pragma once

#include "visionglove/config.hpp"
#include "visionglove/types.hpp"

#include <mutex>
#include <string>
#include <vector>

namespace vg {

class SmsService {
public:
    explicit SmsService(const CommunicationsConfig& cfg);
    bool initialize();
    // Returns true if "sent" (or dry-run logged). Never hits network when dry_run.
    bool send_sms(const std::string& to, const std::string& body);
    [[nodiscard]] const std::vector<std::string>& sent_log() const { return sent_; }

private:
    CommunicationsConfig cfg_;
    bool ok_ = false;
    std::vector<std::string> sent_;
};

class LivestreamService {
public:
    explicit LivestreamService(const LivestreamConfig& cfg);
    bool initialize();
    bool start_stream(const std::string& reason);
    void stop_stream();
    [[nodiscard]] bool streaming() const { return streaming_; }

private:
    LivestreamConfig cfg_;
    bool ok_ = false;
    bool streaming_ = false;
};

class EmergencyDispatcher {
public:
    EmergencyDispatcher(const CommunicationsConfig& comm, const LivestreamConfig& stream);

    bool initialize();
    void stop();
    [[nodiscard]] bool active() const { return active_; }

    bool dispatch(ThreatLevel level, const std::string& location = "Unknown");

    [[nodiscard]] const std::vector<EmergencyEvent>& history() const { return history_; }
    [[nodiscard]] bool test_systems(std::vector<std::pair<std::string, bool>>& results);

private:
    void handle_caution(EmergencyEvent& e);
    void handle_alert(EmergencyEvent& e);
    void handle_emergency(EmergencyEvent& e);
    static std::string make_id();

    CommunicationsConfig comm_;
    LivestreamConfig stream_cfg_;
    SmsService sms_;
    LivestreamService live_;
    bool active_ = false;
    std::mutex mu_;
    std::vector<EmergencyEvent> history_;
    EmergencyEvent* current_ = nullptr;
};

}  // namespace vg
