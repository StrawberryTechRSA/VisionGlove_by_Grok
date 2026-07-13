#pragma once

#include "visionglove/comms.hpp"
#include "visionglove/config.hpp"
#include "visionglove/haptics.hpp"
#include "visionglove/security.hpp"
#include "visionglove/sensors.hpp"
#include "visionglove/types.hpp"
#include "visionglove/vision.hpp"

#include <atomic>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>

namespace vg {

class GloveSystem {
public:
    explicit GloveSystem(AppConfig cfg);
    ~GloveSystem();

    GloveSystem(const GloveSystem&) = delete;
    GloveSystem& operator=(const GloveSystem&) = delete;

    bool initialize();
    bool start();
    void stop();

    [[nodiscard]] SystemStatus status() const;
    [[nodiscard]] ThreatLevel threat_level() const {
        return threat_.load(std::memory_order_acquire);
    }

    // Demo / test hooks
    void set_scenario(const std::string& name);
    void set_person_count(int n);
    bool run_self_test(std::vector<std::pair<std::string, bool>>& results);

    SensorManager* sensors() { return sensors_.get(); }
    VisionProcessor* vision() { return vision_.get(); }
    EmergencyDispatcher* emergency() { return emergency_.get(); }

private:
    void main_loop();
    ThreatLevel fuse(const SensorSnapshot& s, const VisionSnapshot& v) const;
    void on_threat_change(ThreatLevel prev, ThreatLevel next);

    AppConfig cfg_;
    std::unique_ptr<AuthManager> auth_;
    std::unique_ptr<SensorManager> sensors_;
    std::unique_ptr<VisionProcessor> vision_;
    std::unique_ptr<HapticController> haptics_;
    std::unique_ptr<EmergencyDispatcher> emergency_;

    std::atomic<bool> running_{false};
    std::atomic<bool> stop_{false};
    std::thread loop_;
    TimePoint start_time_{};
    std::atomic<ThreatLevel> threat_{ThreatLevel::Safe};
    std::atomic<std::uint64_t> cycles_{0};
    mutable std::mutex status_mu_;
};

}  // namespace vg
