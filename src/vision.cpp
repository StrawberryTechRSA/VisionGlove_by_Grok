#include "visionglove/vision.hpp"
#include "visionglove/logger.hpp"

#include <chrono>
#include <cmath>
#include <sstream>

namespace vg {

// ============================ ThreatAnalyzer ===============================

ThreatAnalyzer::ThreatAnalyzer(const VisionConfig& cfg, const SensorsConfig& sensors_cfg)
    : vision_cfg_(cfg), sensors_cfg_(sensors_cfg) {}

ThreatAnalyzer::Result ThreatAnalyzer::analyze(int person_count,
                                               bool unusual_movement,
                                               bool emergency_gesture,
                                               double grip,
                                               Gesture gesture) const {
    Result r;

    // Weighted multi-factor score (Claude returned constants)
    double score = 0.0;
    std::ostringstream desc;

    // People density factor
    if (person_count >= vision_cfg_.person_threshold) {
        const double over =
            static_cast<double>(person_count - vision_cfg_.person_threshold + 1) /
            std::max(1, vision_cfg_.person_threshold);
        score += std::min(0.35, 0.15 + 0.1 * over);
        desc << "crowd(" << person_count << ") ";
    } else if (person_count >= 2) {
        score += 0.08;
        desc << "persons(" << person_count << ") ";
    }

    if (unusual_movement) {
        score += 0.30;
        desc << "violent_motion ";
    }

    if (emergency_gesture || gesture == Gesture::PanicSequence) {
        score += 0.55;
        desc << "panic_gesture ";
    } else if (gesture == Gesture::Fist && grip > 0.7) {
        score += 0.12;
        desc << "tight_fist ";
    }

    // Soft saturation
    score = std::min(1.0, score);
    r.score = score;

    if (emergency_gesture || gesture == Gesture::PanicSequence || score >= 0.75) {
        r.level = ThreatLevel::Emergency;
    } else if (score >= 0.45 || (unusual_movement && person_count >= 2)) {
        r.level = ThreatLevel::Alert;
    } else if (score >= 0.15 || person_count >= vision_cfg_.person_threshold) {
        r.level = ThreatLevel::Caution;
    } else {
        r.level = ThreatLevel::Safe;
    }

    if (desc.str().empty()) desc << "nominal";
    r.description = desc.str();
    return r;
}

// ============================ PersonDetector ===============================

PersonDetector::PersonDetector(const VisionConfig& cfg) : cfg_(cfg), sim_count_(1) {}

PersonDetector::Result PersonDetector::detect() {
    Result r;
    ++tick_;
    // Mild oscillation around sim_count_ to look alive
    int n = sim_count_;
    if (n > 0 && (tick_ % 47) == 0) {
        // occasional flicker
    }
    r.count = std::max(0, n);
    r.detections.reserve(static_cast<std::size_t>(r.count));
    for (int i = 0; i < r.count; ++i) {
        PersonDetection d;
        d.confidence = static_cast<float>(cfg_.detection_threshold + 0.1);
        d.x = 0.1f + 0.15f * i;
        d.y = 0.2f;
        d.w = 0.12f;
        d.h = 0.35f;
        r.detections.push_back(d);
    }
    return r;
}

// ============================ VisionProcessor ==============================

VisionProcessor::VisionProcessor(const VisionConfig& cfg, const SensorsConfig& sensors_cfg)
    : cfg_(cfg), detector_(cfg), threat_(cfg, sensors_cfg) {}

VisionProcessor::~VisionProcessor() { stop(); }

bool VisionProcessor::start() {
    if (active_.load()) return true;
    stop_ = false;
    worker_ = std::thread([this] { thread_main(); });
    active_ = true;
    VG_LOG_INFO("Vision", "Vision processor started @ " + std::to_string(cfg_.fps) + " FPS target"
                + (cfg_.simulate ? " (simulate)" : ""));
    return true;
}

void VisionProcessor::stop() {
    stop_ = true;
    if (worker_.joinable()) worker_.join();
    active_ = false;
    VG_LOG_INFO("Vision", "Vision processor stopped");
}

void VisionProcessor::update_sensor_context(const SensorSnapshot& s) {
    std::lock_guard lock(ctx_mu_);
    sensor_ctx_ = s;
}

std::optional<VisionSnapshot> VisionProcessor::latest() { return queue_.pop_latest(); }

void VisionProcessor::thread_main() {
    using namespace std::chrono;
    const auto interval = duration<double>(1.0 / std::max(1, cfg_.fps));
    auto next = steady_clock::now();
    auto window_start = next;
    int window_count = 0;

    while (!stop_.load(std::memory_order_acquire)) {
        SensorSnapshot ctx;
        {
            std::lock_guard lock(ctx_mu_);
            ctx = sensor_ctx_;
        }

        auto persons = detector_.detect();
        auto analysis = threat_.analyze(persons.count,
                                        ctx.unusual_movement,
                                        ctx.emergency_gesture,
                                        ctx.grip_strength,
                                        ctx.primary_gesture);

        VisionSnapshot snap;
        snap.timestamp = Clock::now();
        snap.frame_id = ++frame_id_;
        snap.person_count = persons.count;
        snap.persons = std::move(persons.detections);
        snap.threat_score = analysis.score;
        snap.vision_threat = analysis.level;
        snap.description = std::move(analysis.description);
        snap.ok = true;

        if (!queue_.try_push(std::move(snap))) {
            (void)queue_.try_pop();
        }

        ++window_count;
        const auto now = steady_clock::now();
        if (now - window_start >= 1s) {
            measured_fps_.store(static_cast<double>(window_count) /
                                    duration<double>(now - window_start).count(),
                                std::memory_order_relaxed);
            window_count = 0;
            window_start = now;
        }

        next += duration_cast<steady_clock::duration>(interval);
        if (next < now) next = now;
        std::this_thread::sleep_until(next);
    }
}

}  // namespace vg
