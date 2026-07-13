#include "visionglove/sensors.hpp"
#include "visionglove/logger.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <numeric>

namespace vg {
namespace {
constexpr double kPi = 3.14159265358979323846;

double clamp01(double v) { return std::clamp(v, 0.0, 1.0); }

double sim_noise(double t, int seed) {
    return 0.02 * std::sin(t * 7.1 + seed) + 0.01 * std::sin(t * 13.3 + seed * 2);
}
}  // namespace

// ============================ FlexSensor ===================================

FlexSensor::FlexSensor(int finger_id, const SensorsConfig& cfg)
    : finger_id_(finger_id), cfg_(cfg) {}

void FlexSensor::inject_raw(double raw) {
    // raw < 0 clears injection and returns to simulation/hardware path
    injected_ = (raw < 0.0) ? -1.0 : clamp01(raw);
}

FlexReading FlexSensor::read() {
    FlexReading r;
    double raw = 0;
    if (injected_ >= 0) {
        raw = injected_;
    } else if (simulate_) {
        using namespace std::chrono;
        const double t = duration<double>(steady_clock::now().time_since_epoch()).count();
        const double base = 0.15 + finger_id_ * 0.08;
        raw = clamp01(base + 0.25 * std::sin(t * 1.3 + finger_id_) + sim_noise(t, finger_id_));
    } else {
        r.ok = false;
        return r;
    }
    // EMA denoise
    ema_ = ema_alpha_ * raw + (1.0 - ema_alpha_) * ema_;
    r.raw = ema_;
    if (calibrated_ && max_v_ > min_v_) {
        r.value = clamp01((ema_ - min_v_) / (max_v_ - min_v_));
    } else {
        r.value = ema_;
    }
    r.ok = true;
    return r;
}

void FlexSensor::calibrate_sample(double raw) {
    min_v_ = std::min(min_v_ == 0 && max_v_ == 1 && !calibrated_ ? raw : min_v_, raw);
    max_v_ = std::max(max_v_, raw);
    // first sample init
    if (!calibrated_ && min_v_ == 0 && max_v_ == 1) {
        min_v_ = raw;
        max_v_ = raw;
    }
}

void FlexSensor::finalize_calibration() {
    if (max_v_ - min_v_ < 1e-3) {
        min_v_ = 0;
        max_v_ = 1;
    }
    calibrated_ = true;
}

// ============================ ImuSensor ====================================

ImuSensor::ImuSensor(const SensorsConfig& cfg) : cfg_(cfg) {
    last_ = Clock::now();
}

void ImuSensor::inject(const Vec3& accel, const Vec3& gyro, const Vec3& mag) {
    inj_accel = accel;
    inj_gyro = gyro;
    inj_mag = mag;
    has_inject_ = true;
}

ImuReading ImuSensor::read(double dt) {
    ImuReading r;
    Vec3 accel, gyro, mag;

    if (has_inject_) {
        accel = inj_accel;
        gyro = inj_gyro;
        mag = inj_mag;
    } else if (simulate_) {
        using namespace std::chrono;
        const double t = duration<double>(steady_clock::now().time_since_epoch()).count();
        accel = {2.0 * std::sin(t * 2.0), 1.5 * std::cos(t * 1.5), 9.81 + 0.5 * std::sin(t * 3.0)};
        gyro = {0.1 * std::sin(t), 0.08 * std::cos(t * 1.2), 0.05 * std::sin(t * 0.7)};
        mag = {0.3, 0.1, 0.4};
    } else {
        r.ok = false;
        return r;
    }

    accel = accel - accel_bias_;
    gyro = gyro - gyro_bias_;
    mag = mag - mag_bias_;

    if (dt <= 0 || dt > 0.5) dt = 0.01;

    // Complementary filter: integrate gyro, correct tilt with accelerometer
    q_ = q_.integrate_gyro(gyro, dt);

    // Accel-derived tilt quaternion (gravity direction)
    Vec3 an = accel.normalized();
    if (an.norm() > 0.5) {
        // Estimated gravity direction from current orientation
        // Rotation matrix third column (body z in world) ... use simplified correction
        // Build orientation error between measured gravity and predicted
        const Vec3 g_est = {
            2 * (q_.x * q_.z - q_.w * q_.y),
            2 * (q_.y * q_.z + q_.w * q_.x),
            q_.w * q_.w - q_.x * q_.x - q_.y * q_.y + q_.z * q_.z
        };
        const Vec3 err = an.cross(g_est);  // proportional error
        // Small corrective angular velocity
        const Vec3 corr = err * ((1.0 - alpha_) * 2.0);
        q_ = q_.integrate_gyro(corr, dt);
    }

    r.accel = accel;
    r.gyro = gyro;
    r.mag = mag;
    r.orientation = q_;
    r.euler_deg = q_.to_euler_deg();
    r.accel_mag = accel.norm();
    r.gyro_mag = gyro.norm();
    r.ok = true;
    return r;
}

// ============================ PressureSensor ===============================

PressureSensor::PressureSensor(int location_id, const SensorsConfig& cfg)
    : location_id_(location_id), cfg_(cfg) {}

void PressureSensor::inject_raw(double raw) {
    injected_ = (raw < 0.0) ? -1.0 : clamp01(raw);
}

PressureReading PressureSensor::read() {
    PressureReading r;
    double raw = 0;
    if (injected_ >= 0) {
        raw = injected_;
    } else if (simulate_) {
        using namespace std::chrono;
        const double t = duration<double>(steady_clock::now().time_since_epoch()).count();
        raw = clamp01(0.2 + 0.15 * std::sin(t * 0.9 + location_id_) + sim_noise(t, location_id_ + 10));
    } else {
        r.ok = false;
        return r;
    }
    ema_ = 0.3 * raw + 0.7 * ema_;
    r.raw = ema_;
    r.value = ema_;
    r.ok = true;
    return r;
}

// ============================ GestureEngine ================================

GestureEngine::GestureEngine(const SensorsConfig& cfg) : cfg_(cfg) {}

Gesture GestureEngine::classify(const std::array<double, 5>& flex) const {
    const double cth = cfg_.closed_threshold;
    const double oth = cfg_.open_threshold;

    int closed = 0, open = 0;
    for (double v : flex) {
        if (v > cth) ++closed;
        if (v < oth) ++open;
    }

    // Thumbs up: thumb open, others closed
    if (flex[0] < oth && flex[1] > cth && flex[2] > cth && flex[3] > cth && flex[4] > cth)
        return Gesture::ThumbsUp;

    if (closed == 5) return Gesture::Fist;
    if (open == 5) return Gesture::OpenHand;

    // Pointing: index open, others closed
    if (flex[1] < oth && flex[0] > cth * 0.8 && flex[2] > cth && flex[3] > cth && flex[4] > cth)
        return Gesture::Pointing;

    // Peace: index+middle open
    if (flex[1] < oth && flex[2] < oth && flex[0] > cth * 0.7 && flex[3] > cth && flex[4] > cth)
        return Gesture::Peace;

    return Gesture::None;
}

Gesture GestureEngine::update(const std::array<double, 5>& flex,
                              double movement_mag,
                              std::vector<Gesture>& out_all) {
    out_all.clear();
    const Gesture raw = classify(flex);

    // Hysteresis: require k consecutive same classifications
    if (raw == last_stable_) {
        hysteresis_count_ = kHysteresis;
    } else {
        if (raw != Gesture::None) {
            ++hysteresis_count_;
            if (hysteresis_count_ >= kHysteresis) {
                // Transition into new gesture
                if (raw == Gesture::Fist && last_stable_ != Gesture::Fist) {
                    fist_times_.push_back(Clock::now());
                }
                last_stable_ = raw;
                hysteresis_count_ = 0;
            }
        } else {
            hysteresis_count_ = 0;
            last_stable_ = Gesture::None;
        }
    }

    if (last_stable_ != Gesture::None) out_all.push_back(last_stable_);

    // Panic sequence: N fists within window
    const auto now = Clock::now();
    const auto window = std::chrono::duration<double>(cfg_.panic_window_s);
    fist_times_.erase(std::remove_if(fist_times_.begin(), fist_times_.end(),
                                     [&](TimePoint t) { return now - t > window; }),
                      fist_times_.end());
    if (static_cast<int>(fist_times_.size()) >= cfg_.panic_fist_count) {
        emergency_ = true;
        out_all.push_back(Gesture::PanicSequence);
        fist_times_.clear();
    }

    // Violent fist = emergency
    if (last_stable_ == Gesture::Fist && movement_mag > cfg_.unusual_accel_threshold * 0.7) {
        emergency_ = true;
        out_all.push_back(Gesture::PanicSequence);
    }

    if (emergency_ && last_stable_ != Gesture::PanicSequence)
        return Gesture::PanicSequence;
    return last_stable_;
}

// ============================ SensorManager ================================

SensorManager::SensorManager(const SensorsConfig& cfg)
    : cfg_(cfg),
      flex_{FlexSensor{0, cfg}, FlexSensor{1, cfg}, FlexSensor{2, cfg},
            FlexSensor{3, cfg}, FlexSensor{4, cfg}},
      imu_(cfg),
      pressure_{PressureSensor{0, cfg}, PressureSensor{1, cfg}, PressureSensor{2, cfg},
                PressureSensor{3, cfg}, PressureSensor{4, cfg}},
      gestures_(cfg) {}

SensorManager::~SensorManager() { stop(); }

void SensorManager::set_simulation(bool on) {
    for (auto& f : flex_) f.set_simulation(on);
    imu_.set_simulation(on);
    for (auto& p : pressure_) p.set_simulation(on);
}

void SensorManager::force_gesture_scenario(const std::string& name) {
    std::lock_guard lock(scenario_mu_);
    scenario_ = name;
}

bool SensorManager::start() {
    if (active_.load()) return true;
    stop_ = false;
    worker_ = std::thread([this] { thread_main(); });
    active_ = true;
    VG_LOG_INFO("Sensors", "Sensor manager started at " + std::to_string(cfg_.sample_rate) + " Hz target");
    return true;
}

void SensorManager::stop() {
    stop_ = true;
    if (worker_.joinable()) worker_.join();
    active_ = false;
    VG_LOG_INFO("Sensors", "Sensor manager stopped");
}

std::optional<SensorSnapshot> SensorManager::latest() { return queue_.pop_latest(); }

void SensorManager::thread_main() {
    using namespace std::chrono;
    const auto interval = duration<double>(1.0 / std::max(1, cfg_.sample_rate));
    auto next = steady_clock::now();
    auto window_start = next;
    int window_count = 0;
    double last_dt = interval.count();

    while (!stop_.load(std::memory_order_acquire)) {
        const auto t0 = steady_clock::now();
        auto snap = collect_once(last_dt);
        snap = process(std::move(snap));
        if (!queue_.try_push(std::move(snap))) {
            // drop oldest by draining one
            (void)queue_.try_pop();
            // retry once
            // (snapshot already moved — re-collect next cycle)
        }
        ++window_count;
        const auto now = steady_clock::now();
        if (now - window_start >= 1s) {
            measured_hz_.store(static_cast<double>(window_count) /
                                   duration<double>(now - window_start).count(),
                               std::memory_order_relaxed);
            window_count = 0;
            window_start = now;
        }
        next += duration_cast<steady_clock::duration>(interval);
        if (next < now) next = now;
        std::this_thread::sleep_until(next);
        last_dt = duration<double>(steady_clock::now() - t0).count();
    }
}

SensorSnapshot SensorManager::collect_once(double dt) {
    SensorSnapshot s;
    s.timestamp = Clock::now();

    std::string scenario;
    {
        std::lock_guard lock(scenario_mu_);
        scenario = scenario_;
    }

    // Scenario injection for demos/tests
    if (scenario == "fist") {
        for (int i = 0; i < 5; ++i) flex_[i].inject_raw(0.92);
        for (int i = 0; i < 5; ++i) pressure_[i].inject_raw(0.7);
    } else if (scenario == "open") {
        for (int i = 0; i < 5; ++i) flex_[i].inject_raw(0.1);
    } else if (scenario == "point") {
        flex_[0].inject_raw(0.85);
        flex_[1].inject_raw(0.1);
        flex_[2].inject_raw(0.9);
        flex_[3].inject_raw(0.9);
        flex_[4].inject_raw(0.9);
    } else if (scenario == "shake") {
        for (int i = 0; i < 5; ++i) flex_[i].inject_raw(0.5);
        imu_.inject({20.0, 5.0, 9.81}, {2.0, 1.5, 0.5}, {0.2, 0.1, 0.3});
    } else if (scenario == "panic") {
        // Alternating open/fist driven by time for sequence detection
        using namespace std::chrono;
        const auto ms = duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
        const bool closed = ((ms / 250) % 2) == 0;
        for (int i = 0; i < 5; ++i) flex_[i].inject_raw(closed ? 0.95 : 0.1);
        imu_.inject({12.0, 3.0, 9.81}, {1.0, 0.5, 0.2}, {0.2, 0.1, 0.3});
    } else {
        // idle: clear inject so simulation waveform is used
        for (int i = 0; i < 5; ++i) flex_[i].inject_raw(-1.0);
        for (int i = 0; i < 5; ++i) pressure_[i].inject_raw(-1.0);
        imu_.clear_inject();
    }

    for (int i = 0; i < 5; ++i) s.flex[i] = flex_[i].read();
    s.imu = imu_.read(dt);
    for (int i = 0; i < 5; ++i) s.pressure[i] = pressure_[i].read();
    return s;
}

SensorSnapshot SensorManager::process(SensorSnapshot raw) {
    std::array<double, 5> flex_v{};
    for (int i = 0; i < 5; ++i) flex_v[i] = raw.flex[i].value;
    raw.hand_closure = std::accumulate(flex_v.begin(), flex_v.end(), 0.0) / 5.0;

    double psum = 0;
    for (auto& p : raw.pressure) psum += p.value;
    raw.grip_strength = psum / 5.0;

    raw.movement_magnitude = raw.imu.accel_mag;
    raw.unusual_movement = raw.imu.accel_mag > cfg_.unusual_accel_threshold;

    raw.primary_gesture = gestures_.update(flex_v, raw.movement_magnitude, raw.gestures);
    raw.emergency_gesture = gestures_.emergency_flag() ||
                            (raw.primary_gesture == Gesture::PanicSequence);
    return raw;
}

bool SensorManager::calibrate(int samples) {
    VG_LOG_INFO("Sensors", "Calibrating...");
    for (auto& f : flex_) {
        // reset calibration bounds
        // collect samples
        for (int i = 0; i < samples; ++i) {
            auto r = f.read();
            f.calibrate_sample(r.raw);
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        f.finalize_calibration();
    }
    VG_LOG_INFO("Sensors", "Calibration complete");
    return true;
}

}  // namespace vg
