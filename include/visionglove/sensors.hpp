#pragma once

#include "visionglove/config.hpp"
#include "visionglove/ring_buffer.hpp"
#include "visionglove/serial_stub.hpp"
#include "visionglove/types.hpp"

#include <atomic>
#include <memory>
#include <mutex>
#include <thread>

namespace vg {

// ---------------------------------------------------------------------------
// Flex sensor: EMA filter + min/max calibration (Claude only did linear map)
// ---------------------------------------------------------------------------
class FlexSensor {
public:
    explicit FlexSensor(int finger_id, const SensorsConfig& cfg);

    void set_simulation(bool on) { simulate_ = on; }
    void inject_raw(double raw);  // for tests / hardware bridge

    [[nodiscard]] FlexReading read();
    void calibrate_sample(double raw);
    void finalize_calibration();
    [[nodiscard]] bool calibrated() const { return calibrated_; }

private:
    int finger_id_ = 0;
    SensorsConfig cfg_;
    bool simulate_ = true;
    bool calibrated_ = false;
    double min_v_ = 0.0;
    double max_v_ = 1.0;
    double ema_ = 0.0;
    double injected_ = -1.0;
    double ema_alpha_ = 0.25;
};

// ---------------------------------------------------------------------------
// IMU: complementary filter (gyro + accel) — Claude used broken Euler stubs
// ---------------------------------------------------------------------------
class ImuSensor {
public:
    explicit ImuSensor(const SensorsConfig& cfg);

    void set_simulation(bool on) { simulate_ = on; }
    void inject(const Vec3& accel, const Vec3& gyro, const Vec3& mag);
    void clear_inject() { has_inject_ = false; }

    [[nodiscard]] ImuReading read(double dt);
    void set_accel_bias(const Vec3& b) { accel_bias_ = b; }
    void set_gyro_bias(const Vec3& b) { gyro_bias_ = b; }

private:
    SensorsConfig cfg_;
    bool simulate_ = true;
    bool has_inject_ = false;
    Vec3 inj_accel{}, inj_gyro{}, inj_mag{};
    Vec3 accel_bias_{}, gyro_bias_{}, mag_bias_{};
    Quat q_{};
    double alpha_ = 0.98;  // complementary filter weight on gyro
    TimePoint last_{};
};

// ---------------------------------------------------------------------------
// Pressure sensor
// ---------------------------------------------------------------------------
class PressureSensor {
public:
    explicit PressureSensor(int location_id, const SensorsConfig& cfg);
    void set_simulation(bool on) { simulate_ = on; }
    void inject_raw(double raw);
    [[nodiscard]] PressureReading read();

private:
    int location_id_ = 0;
    SensorsConfig cfg_;
    bool simulate_ = true;
    double injected_ = -1.0;
    double ema_ = 0.0;
};

// ---------------------------------------------------------------------------
// Gesture FSM with hysteresis + panic sequence detection
// ---------------------------------------------------------------------------
class GestureEngine {
public:
    explicit GestureEngine(const SensorsConfig& cfg);

    // Returns primary gesture and fills list of active gestures.
    Gesture update(const std::array<double, 5>& flex,
                   double movement_mag,
                   std::vector<Gesture>& out_all);

    [[nodiscard]] bool emergency_flag() const { return emergency_; }
    void reset_emergency() { emergency_ = false; }

private:
    SensorsConfig cfg_;
    Gesture last_stable_ = Gesture::None;
    int hysteresis_count_ = 0;
    static constexpr int kHysteresis = 3;

    // Panic: N fists within window
    std::vector<TimePoint> fist_times_;
    bool emergency_ = false;

    [[nodiscard]] Gesture classify(const std::array<double, 5>& flex) const;
};

// ---------------------------------------------------------------------------
// Sensor manager: dedicated high-rate thread + SPSC handoff
// ---------------------------------------------------------------------------
class SensorManager {
public:
    explicit SensorManager(const SensorsConfig& cfg);
    ~SensorManager();

    SensorManager(const SensorManager&) = delete;
    SensorManager& operator=(const SensorManager&) = delete;

    bool start();
    void stop();
    [[nodiscard]] bool active() const { return active_.load(std::memory_order_acquire); }

    // Non-blocking: latest snapshot for fusion
    [[nodiscard]] std::optional<SensorSnapshot> latest();

    // Test / demo control
    void set_simulation(bool on);
    void force_gesture_scenario(const std::string& name);  // "idle","fist","panic","shake","serial"
    bool calibrate(int samples = 50);

    // Hardware bridge stub: text file or future COM port using same line protocol
    bool attach_serial_feed(const std::string& path, bool loop = true);
    void detach_serial_feed();
    [[nodiscard]] SerialStub* serial_stub() { return serial_.get(); }

    [[nodiscard]] double measured_hz() const { return measured_hz_.load(std::memory_order_relaxed); }

private:
    void thread_main();
    SensorSnapshot collect_once(double dt);
    SensorSnapshot process(SensorSnapshot raw);

    SensorsConfig cfg_;
    std::array<FlexSensor, 5> flex_;
    ImuSensor imu_;
    std::array<PressureSensor, 5> pressure_;
    GestureEngine gestures_;

    SpscRingBuffer<SensorSnapshot, 64> queue_;
    std::atomic<bool> active_{false};
    std::atomic<bool> stop_{false};
    std::thread worker_;
    std::atomic<double> measured_hz_{0};

    std::mutex scenario_mu_;
    std::string scenario_ = "idle";

    std::unique_ptr<SerialStub> serial_;
    int serial_pump_div_ = 0;  // pump file every N collect cycles
};

}  // namespace vg
