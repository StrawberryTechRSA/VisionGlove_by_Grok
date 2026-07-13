#pragma once

#include "visionglove/config.hpp"
#include "visionglove/ring_buffer.hpp"
#include "visionglove/types.hpp"

#include <atomic>
#include <string>
#include <thread>

namespace vg {

// Multi-factor threat scorer — Claude returned constant zeros.
class ThreatAnalyzer {
public:
    explicit ThreatAnalyzer(const VisionConfig& cfg, const SensorsConfig& sensors_cfg);

    // Continuous score [0,1] + mapped ThreatLevel + human description
    struct Result {
        double score = 0;
        ThreatLevel level = ThreatLevel::Safe;
        std::string description;
    };

    [[nodiscard]] Result analyze(int person_count,
                                 bool unusual_movement,
                                 bool emergency_gesture,
                                 double grip,
                                 Gesture gesture) const;

private:
    VisionConfig vision_cfg_;
    SensorsConfig sensors_cfg_;
};

// Simulated person detector with controllable density (no OpenCV required).
// Architecture leaves room for a real backend (YOLO/OpenCV) later.
class PersonDetector {
public:
    explicit PersonDetector(const VisionConfig& cfg);

    void set_simulated_count(int n) { sim_count_ = n; }
    [[nodiscard]] int simulated_count() const { return sim_count_; }

    struct Result {
        int count = 0;
        std::vector<PersonDetection> detections;
    };

    [[nodiscard]] Result detect();  // uses simulation or future CV backend

private:
    VisionConfig cfg_;
    int sim_count_ = 1;
    std::uint64_t tick_ = 0;
};

class VisionProcessor {
public:
    explicit VisionProcessor(const VisionConfig& cfg, const SensorsConfig& sensors_cfg);
    ~VisionProcessor();

    VisionProcessor(const VisionProcessor&) = delete;
    VisionProcessor& operator=(const VisionProcessor&) = delete;

    bool start();
    void stop();
    [[nodiscard]] bool active() const { return active_.load(std::memory_order_acquire); }

    // Feed latest sensor context for multi-modal threat analysis
    void update_sensor_context(const SensorSnapshot& s);

    [[nodiscard]] std::optional<VisionSnapshot> latest();
    [[nodiscard]] double measured_fps() const { return measured_fps_.load(std::memory_order_relaxed); }

    void set_simulated_person_count(int n) { detector_.set_simulated_count(n); }
    ThreatAnalyzer& threat() { return threat_; }

private:
    void thread_main();

    VisionConfig cfg_;
    PersonDetector detector_;
    ThreatAnalyzer threat_;

    std::mutex ctx_mu_;
    SensorSnapshot sensor_ctx_{};

    SpscRingBuffer<VisionSnapshot, 32> queue_;
    std::atomic<bool> active_{false};
    std::atomic<bool> stop_{false};
    std::thread worker_;
    std::atomic<double> measured_fps_{0};
    std::uint64_t frame_id_ = 0;
};

}  // namespace vg
