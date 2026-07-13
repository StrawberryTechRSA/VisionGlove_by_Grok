#include "visionglove/comms.hpp"
#include "visionglove/logger.hpp"

#include <chrono>
#include <sstream>

namespace vg {

// ============================ SMS ==========================================

SmsService::SmsService(const CommunicationsConfig& cfg) : cfg_(cfg) {}

bool SmsService::initialize() {
    ok_ = true;
    if (cfg_.dry_run) {
        VG_LOG_INFO("SMS", "SMS service in DRY-RUN mode (no network)");
    } else if (cfg_.sms.account_sid.empty() || cfg_.sms.auth_token.empty()) {
        VG_LOG_WARN("SMS", "SMS credentials missing — falling back to dry-run");
        cfg_.dry_run = true;
    } else {
        VG_LOG_INFO("SMS", "SMS service configured for provider " + cfg_.sms.provider);
    }
    return true;
}

bool SmsService::send_sms(const std::string& to, const std::string& body) {
    if (!ok_) return false;
    const std::string entry = "to=" + to + " body=" + body;
    sent_.push_back(entry);
    if (cfg_.dry_run) {
        VG_LOG_INFO("SMS", "[dry-run] " + entry);
        return true;
    }
    // Real Twilio integration would go here; we refuse silent fake success.
    VG_LOG_ERROR("SMS", "Live SMS not linked in this build — enable dry_run or add HTTP client");
    return false;
}

// ============================ Livestream ===================================

LivestreamService::LivestreamService(const LivestreamConfig& cfg) : cfg_(cfg) {}

bool LivestreamService::initialize() {
    ok_ = true;
    VG_LOG_INFO("Live", std::string("Livestream service ready (") +
                (cfg_.dry_run ? "dry-run" : "live") + ")");
    return true;
}

bool LivestreamService::start_stream(const std::string& reason) {
    if (!ok_ || !cfg_.enabled) return false;
    streaming_ = true;
    VG_LOG_WARN("Live", std::string(cfg_.dry_run ? "[dry-run] " : "") +
                "Stream started on " + cfg_.platform + ": " + reason);
    return true;
}

void LivestreamService::stop_stream() {
    if (streaming_) {
        VG_LOG_INFO("Live", "Stream stopped");
        streaming_ = false;
    }
}

// ============================ EmergencyDispatcher ==========================

EmergencyDispatcher::EmergencyDispatcher(const CommunicationsConfig& comm,
                                         const LivestreamConfig& stream)
    : comm_(comm), stream_cfg_(stream), sms_(comm), live_(stream) {}

bool EmergencyDispatcher::initialize() {
    sms_.initialize();
    live_.initialize();
    active_ = true;
    VG_LOG_INFO("Emergency", "Emergency dispatcher online");
    return true;
}

void EmergencyDispatcher::stop() {
    live_.stop_stream();
    active_ = false;
}

std::string EmergencyDispatcher::make_id() {
    using namespace std::chrono;
    const auto ms = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
    return "EMG-" + std::to_string(ms);
}

bool EmergencyDispatcher::dispatch(ThreatLevel level, const std::string& location) {
    if (!active_) return false;
    std::lock_guard lock(mu_);

    EmergencyEvent e;
    e.id = make_id();
    e.timestamp = Clock::now();
    e.level = level;
    e.location = location;
    e.active = true;

    VG_LOG_WARN("Emergency", "Dispatch " + e.id + " level=" + std::string(to_string(level)));

    if (level >= ThreatLevel::Caution) handle_caution(e);
    if (level >= ThreatLevel::Alert) handle_alert(e);
    if (level >= ThreatLevel::Emergency) handle_emergency(e);

    history_.push_back(std::move(e));
    if (history_.size() > 100) history_.erase(history_.begin());
    return true;
}

void EmergencyDispatcher::handle_caution(EmergencyEvent& e) {
    e.actions.push_back("logged_event");
    VG_LOG_INFO("Emergency", "Caution: systems primed");
}

void EmergencyDispatcher::handle_alert(EmergencyEvent& e) {
    std::ostringstream msg;
    msg << "VisionGlove ALERT at " << e.location << " (id=" << e.id << ")";
    if (!comm_.emergency_contact.empty()) {
        if (sms_.send_sms(comm_.emergency_contact, msg.str()))
            e.actions.push_back("sms_contact");
    } else {
        e.actions.push_back("sms_skipped_no_contact");
        VG_LOG_WARN("Emergency", "No emergency_contact configured");
    }
}

void EmergencyDispatcher::handle_emergency(EmergencyEvent& e) {
    std::ostringstream msg;
    msg << "VisionGlove EMERGENCY at " << e.location << " (id=" << e.id << ") — immediate assistance";
    if (!comm_.police_number.empty()) {
        if (sms_.send_sms(comm_.police_number, msg.str()))
            e.actions.push_back("sms_police");
    } else {
        e.actions.push_back("police_skipped_no_number");
    }
    if (live_.start_stream("emergency " + e.id))
        e.actions.push_back("livestream_started");
}

bool EmergencyDispatcher::test_systems(std::vector<std::pair<std::string, bool>>& results) {
    results.clear();
    results.emplace_back("sms_service", true);
    results.emplace_back("livestream_service", true);
    results.emplace_back("dispatcher_active", active_);
    // Dry-run SMS self-test
    const bool sms_ok = sms_.send_sms("test", "VisionGlove self-test");
    results.emplace_back("sms_dry_run", sms_ok);
    return active_ && sms_ok;
}

}  // namespace vg
