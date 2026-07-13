#include "visionglove/haptics.hpp"
#include "visionglove/logger.hpp"

namespace vg {

HapticController::HapticController(const HapticsConfig& cfg) : cfg_(cfg) {}

bool HapticController::start() {
    active_ = true;
    VG_LOG_INFO("Haptics", "Haptic controller ready");
    return true;
}

void HapticController::stop() {
    active_ = false;
    std::lock_guard lock(mu_);
    remaining_ms_ = 0;
}

HapticPattern HapticController::pattern_for(ThreatLevel level, double intensity) {
    HapticPattern p;
    p.intensity = intensity;
    switch (level) {
        case ThreatLevel::Safe:
            p.name = "none";
            break;
        case ThreatLevel::Caution:
            p.name = "gentle_pulse";
            p.segments = {{100, 200}, {100, 200}};
            break;
        case ThreatLevel::Alert:
            p.name = "rapid_pulse";
            p.segments = {{50, 50}, {50, 50}, {50, 50}, {50, 50}};
            break;
        case ThreatLevel::Emergency:
            p.name = "continuous_buzz";
            p.segments = {{500, 50}, {500, 50}, {500, 50}};
            break;
    }
    return p;
}

void HapticController::threat_feedback(ThreatLevel level) {
    if (!active_ || !cfg_.enabled) return;
    auto p = pattern_for(level, cfg_.intensity);
    int total = 0;
    for (auto& seg : p.segments) total += seg.first + seg.second;

    std::lock_guard lock(mu_);
    current_ = std::move(p);
    remaining_ms_ = total;
    last_pattern_ = current_.name;
    VG_LOG_INFO("Haptics", "Pattern '" + last_pattern_ + "' for threat " +
                std::string(to_string(level)));
}

std::string HapticController::last_pattern() const {
    std::lock_guard lock(mu_);
    return last_pattern_;
}

void HapticController::tick_for_test(int ms) {
    std::lock_guard lock(mu_);
    remaining_ms_ = std::max(0, remaining_ms_ - ms);
}

}  // namespace vg
