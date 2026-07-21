#include "visionglove/config.hpp"
#include "visionglove/glove_system.hpp"
#include "visionglove/logger.hpp"

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstring>
#include <iostream>
#include <string>
#include <thread>

namespace {
std::atomic<bool> g_run{true};

void on_signal(int) { g_run = false; }

void print_usage(const char* argv0) {
    std::cout
        << "VisionGlove by Grok — C++20 cybernetic safety system\n"
        << "Usage: " << argv0 << " [options]\n"
        << "  --config, -c <path>   Configuration JSON\n"
        << "  --debug, -d           Debug logging\n"
        << "  --test, -t            Run self-tests and exit\n"
        << "  --demo <name>         Scenario: idle|fist|open|point|rock|shake|panic|serial\n"
        << "  --serial-file <path>  Text sensor feed (FLEX/IMU lines); enables serial scenario\n"
        << "  --persons <n>         Simulated person count\n"
        << "  --seconds <n>         Run for n seconds then exit (0=forever)\n"
        << "  --version, -v         Version\n"
        << "  --help, -h            Help\n";
}
}  // namespace

int main(int argc, char** argv) {
    std::string config_path = "config/config.json";
    bool debug = false;
    bool test_only = false;
    std::string demo = "idle";
    std::string serial_file;
    int persons = 1;
    int seconds = 0;

    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if ((a == "--config" || a == "-c") && i + 1 < argc) config_path = argv[++i];
        else if (a == "--debug" || a == "-d") debug = true;
        else if (a == "--test" || a == "-t") test_only = true;
        else if (a == "--demo" && i + 1 < argc) demo = argv[++i];
        else if (a == "--serial-file" && i + 1 < argc) serial_file = argv[++i];
        else if (a == "--persons" && i + 1 < argc) persons = std::stoi(argv[++i]);
        else if (a == "--seconds" && i + 1 < argc) seconds = std::stoi(argv[++i]);
        else if (a == "--version" || a == "-v") {
            std::cout << "VisionGlove by Grok v2.0.0-grok (C++20)\n";
            return 0;
        } else if (a == "--help" || a == "-h") {
            print_usage(argv[0]);
            return 0;
        } else {
            std::cerr << "Unknown argument: " << a << "\n";
            print_usage(argv[0]);
            return 2;
        }
    }

    vg::Logger::instance().set_level(debug ? vg::LogLevel::Debug : vg::LogLevel::Info);
    vg::Logger::instance().set_file("logs/vision_glove.log");

    bool cfg_ok = false;
    auto cfg = vg::load_config(config_path, &cfg_ok);
    if (debug) cfg.system.debug_mode = true;

    vg::GloveSystem system(cfg);

    if (test_only) {
        std::vector<std::pair<std::string, bool>> results;
        const bool pass = system.run_self_test(results);
        int failed = 0;
        for (auto& [n, p] : results) if (!p) ++failed;
        std::cout << "\nSelf-test: " << (pass ? "PASSED" : "FAILED")
                  << " (" << (results.size() - failed) << "/" << results.size() << ")\n";
        return pass ? 0 : 1;
    }

    std::signal(SIGINT, on_signal);
#ifndef _WIN32
    std::signal(SIGTERM, on_signal);
#endif

    if (!system.initialize() || !system.start()) {
        VG_LOG_ERROR("Main", "Failed to start VisionGlove");
        return 1;
    }

    if (!serial_file.empty()) {
        if (!system.attach_serial_feed(serial_file, true)) {
            VG_LOG_ERROR("Main", "Failed to open --serial-file: " + serial_file);
            system.stop();
            return 1;
        }
        demo = "serial";
    } else if (demo == "serial") {
        // Default beginner feed
        if (!system.attach_serial_feed("config/sample_serial_feed.txt", true)) {
            VG_LOG_ERROR("Main", "Default serial feed missing: config/sample_serial_feed.txt");
            system.stop();
            return 1;
        }
    } else {
        system.set_scenario(demo);
    }
    system.set_person_count(persons);
    VG_LOG_INFO("Main", "Running demo='" + demo + "' persons=" + std::to_string(persons) +
                (serial_file.empty() ? "" : " serial=" + serial_file));
    VG_LOG_INFO("Main", "Press Ctrl+C to stop");

    using namespace std::chrono;
    const auto t0 = steady_clock::now();
    while (g_run.load()) {
        if (seconds > 0) {
            const double elapsed = duration<double>(steady_clock::now() - t0).count();
            if (elapsed >= static_cast<double>(seconds)) break;
        }

        auto st = system.status();
        if (debug) {
            VG_LOG_DEBUG("Main",
                "uptime=" + std::to_string(st.uptime_s) +
                "s threat=" + std::string(vg::to_string(st.threat)) +
                " sensorHz=" + std::to_string(st.sensor_hz) +
                " visionFps=" + std::to_string(st.vision_fps) +
                " cycles=" + std::to_string(st.cycles));
        }
        std::this_thread::sleep_for(1s);
    }

    system.stop();
    VG_LOG_INFO("Main", "Shutdown complete");
    return 0;
}
