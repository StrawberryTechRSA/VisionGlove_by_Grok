#pragma once

#include "visionglove/config.hpp"
#include "visionglove/types.hpp"

#include <atomic>
#include <mutex>
#include <string>
#include <vector>

namespace vg {

struct HapticPattern {
    std::string name;
    // duty cycle segments: (on_ms, off_ms) repeated
    std::vector<std::pair<int, int>> segments;
    double intensity = 0.8;
};

class HapticController {
public:
    explicit HapticController(const HapticsConfig& cfg);

    bool start();
    void stop();
    [[nodiscard]] bool active() const { return active_; }

    // Non-blocking: schedules pattern for threat level
    void threat_feedback(ThreatLevel level);

    // Returns last activated pattern name (for tests / UI)
    [[nodiscard]] std::string last_pattern() const;

    // Simulate time advance for unit tests without sleeping wall clock
    void tick_for_test(int ms);

private:
    HapticsConfig cfg_;
    bool active_ = false;
    mutable std::mutex mu_;
    std::string last_pattern_;
    HapticPattern current_{};
    int remaining_ms_ = 0;

    static HapticPattern pattern_for(ThreatLevel level, double intensity);
};

}  // namespace vg
