#include "visionglove/glove_system.hpp"
#include "visionglove/logger.hpp"

#include <chrono>

namespace vg {

GloveSystem::GloveSystem(AppConfig cfg) : cfg_(std::move(cfg)) {}

GloveSystem::~GloveSystem() { stop(); }

bool GloveSystem::initialize() {
    std::string err;
    if (!validate_config(cfg_, &err)) {
        VG_LOG_ERROR("System", "Config invalid: " + err);
        return false;
    }

    auth_ = std::make_unique<AuthManager>(cfg_.security);
    if (!auth_->initialize()) return false;

    if (cfg_.sensors.enabled) {
        sensors_ = std::make_unique<SensorManager>(cfg_.sensors);
        sensors_->set_simulation(true);
    }
    if (cfg_.vision.enabled) {
        vision_ = std::make_unique<VisionProcessor>(cfg_.vision, cfg_.sensors);
    }
    if (cfg_.haptics.enabled) {
        haptics_ = std::make_unique<HapticController>(cfg_.haptics);
        haptics_->start();
    }
    emergency_ = std::make_unique<EmergencyDispatcher>(cfg_.communications, cfg_.livestream);
    if (!emergency_->initialize()) return false;

    VG_LOG_INFO("System", "VisionGlove subsystems initialized");
    return true;
}

bool GloveSystem::start() {
    if (running_.load()) return true;
    if (!auth_ && !initialize()) return false;

    if (sensors_ && !sensors_->start()) return false;
    if (vision_ && !vision_->start()) return false;

    stop_ = false;
    start_time_ = Clock::now();
    loop_ = std::thread([this] { main_loop(); });
    running_ = true;
    VG_LOG_INFO("System", "VisionGlove system started");
    return true;
}

void GloveSystem::stop() {
    stop_ = true;
    if (loop_.joinable()) loop_.join();
    if (sensors_) sensors_->stop();
    if (vision_) vision_->stop();
    if (haptics_) haptics_->stop();
    if (emergency_) emergency_->stop();
    if (auth_) auth_->stop();
    running_ = false;
    VG_LOG_INFO("System", "VisionGlove system stopped");
}

void GloveSystem::main_loop() {
    using namespace std::chrono;
    // 50 Hz fusion loop (faster than Claude's 10 Hz asyncio sleep)
    constexpr auto period = 20ms;
    SensorSnapshot last_sensor{};
    VisionSnapshot last_vision{};

    while (!stop_.load(std::memory_order_acquire)) {
        if (sensors_) {
            if (auto s = sensors_->latest()) {
                last_sensor = *s;
                if (vision_) vision_->update_sensor_context(last_sensor);
            }
        }
        if (vision_) {
            if (auto v = vision_->latest()) last_vision = *v;
        }

        const ThreatLevel fused = fuse(last_sensor, last_vision);
        const ThreatLevel prev = threat_.load(std::memory_order_relaxed);
        if (fused != prev) {
            threat_.store(fused, std::memory_order_release);
            on_threat_change(prev, fused);
        }

        cycles_.fetch_add(1, std::memory_order_relaxed);
        std::this_thread::sleep_for(period);
    }
}

ThreatLevel GloveSystem::fuse(const SensorSnapshot& s, const VisionSnapshot& v) const {
    auto raise = [](ThreatLevel cur, ThreatLevel next) {
        return static_cast<int>(next) > static_cast<int>(cur) ? next : cur;
    };

    // Hard escalations
    if (s.emergency_gesture) return ThreatLevel::Emergency;
    if (s.primary_gesture == Gesture::PanicSequence) return ThreatLevel::Emergency;

    ThreatLevel level = ThreatLevel::Safe;

    if (v.ok) level = raise(level, v.vision_threat);

    if (s.unusual_movement) level = raise(level, ThreatLevel::Alert);

    if (v.person_count >= cfg_.vision.person_threshold)
        level = raise(level, ThreatLevel::Caution);

    // Continuous score bridge
    if (v.threat_score >= 0.75) level = raise(level, ThreatLevel::Emergency);
    else if (v.threat_score >= 0.45) level = raise(level, ThreatLevel::Alert);

    return level;
}

void GloveSystem::on_threat_change(ThreatLevel prev, ThreatLevel next) {
    VG_LOG_WARN("System", std::string("Threat ") + std::string(to_string(prev)) + " -> " +
                std::string(to_string(next)));
    if (haptics_) haptics_->threat_feedback(next);
    if (next >= ThreatLevel::Alert && emergency_) {
        emergency_->dispatch(next, "Unknown");
    }
    // Sign the event for integrity
    if (auth_ && auth_->active()) {
        const std::string payload = std::string(to_string(next)) + "|transition";
        const auto sig = auth_->sign(payload);
        VG_LOG_INFO("System", "Event MAC " + sig.substr(0, 16) + "...");
    }
}

SystemStatus GloveSystem::status() const {
    SystemStatus st;
    st.running = running_.load();
    if (st.running) {
        st.uptime_s = std::chrono::duration<double>(Clock::now() - start_time_).count();
    }
    st.threat = threat_.load();
    st.sensors_ok = sensors_ && sensors_->active();
    st.vision_ok = vision_ && vision_->active();
    st.haptics_ok = haptics_ && haptics_->active();
    st.emergency_ok = emergency_ && emergency_->active();
    st.sensor_hz = sensors_ ? sensors_->measured_hz() : 0;
    st.vision_fps = vision_ ? vision_->measured_fps() : 0;
    st.cycles = cycles_.load();
    return st;
}

void GloveSystem::set_scenario(const std::string& name) {
    if (sensors_) sensors_->force_gesture_scenario(name);
}

void GloveSystem::set_person_count(int n) {
    if (vision_) vision_->set_simulated_person_count(n);
}

bool GloveSystem::run_self_test(std::vector<std::pair<std::string, bool>>& results) {
    results.clear();
    std::string err;
    const bool cfg_ok = validate_config(cfg_, &err);
    results.emplace_back("config", cfg_ok);

    if (!initialize()) {
        results.emplace_back("initialize", false);
        return false;
    }
    results.emplace_back("initialize", true);
    results.emplace_back("auth", auth_ && auth_->active());
    results.emplace_back("haptics", haptics_ && haptics_->active());

    if (emergency_) {
        std::vector<std::pair<std::string, bool>> em;
        (void)emergency_->test_systems(em);
        for (auto& p : em) results.push_back(p);
    }

    // Crypto self-test
    if (auth_) {
        const std::string msg = "visionglove-self-test";
        auto sig = auth_->sign(msg);
        results.emplace_back("hmac_sign_verify", auth_->verify(msg, sig));
        results.emplace_back("hmac_tamper_detect", !auth_->verify(msg + "x", sig));
    }

    // Gesture unit path via engine directly
    {
        SensorsConfig sc = cfg_.sensors;
        GestureEngine ge(sc);
        std::vector<Gesture> all;
        std::array<double, 5> fist{{0.9, 0.9, 0.9, 0.9, 0.9}};
        Gesture g = Gesture::None;
        for (int i = 0; i < 5; ++i) g = ge.update(fist, 0.0, all);
        results.emplace_back("gesture_fist", g == Gesture::Fist);

        std::array<double, 5> open{{0.1, 0.1, 0.1, 0.1, 0.1}};
        for (int i = 0; i < 5; ++i) g = ge.update(open, 0.0, all);
        results.emplace_back("gesture_open", g == Gesture::OpenHand);
    }

    // Threat analyzer
    {
        ThreatAnalyzer ta(cfg_.vision, cfg_.sensors);
        auto r0 = ta.analyze(0, false, false, 0.1, Gesture::None);
        results.emplace_back("threat_safe", r0.level == ThreatLevel::Safe);
        auto r3 = ta.analyze(5, true, true, 0.9, Gesture::PanicSequence);
        results.emplace_back("threat_emergency", r3.level == ThreatLevel::Emergency);
    }

    // Ring buffer
    {
        SpscRingBuffer<int, 8> rb;
        results.emplace_back("ring_push", rb.try_push(1) && rb.try_push(2));
        auto a = rb.try_pop();
        auto b = rb.pop_latest();
        results.emplace_back("ring_pop", a && *a == 1 && b && *b == 2);
    }

    // SHA-256 known vector: empty string
    {
        const std::uint8_t empty = 0;
        auto h = sha256(&empty, 0);
        // e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855
        const std::uint8_t expected[32] = {
            0xe3,0xb0,0xc4,0x42,0x98,0xfc,0x1c,0x14,0x9a,0xfb,0xf4,0xc8,0x99,0x6f,0xb9,0x24,
            0x27,0xae,0x41,0xe4,0x64,0x9b,0x93,0x4c,0xa4,0x95,0x99,0x1b,0x78,0x52,0xb8,0x55
        };
        bool ok = true;
        for (int i = 0; i < 32; ++i) if (h[i] != expected[i]) ok = false;
        results.emplace_back("sha256_empty", ok);
    }

    bool all = true;
    for (auto& [name, pass] : results) {
        VG_LOG_INFO("Test", std::string(pass ? "PASS" : "FAIL") + "  " + name);
        if (!pass) all = false;
    }
    return all;
}

}  // namespace vg
