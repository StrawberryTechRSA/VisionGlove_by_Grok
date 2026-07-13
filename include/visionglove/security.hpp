#pragma once

#include "visionglove/config.hpp"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace vg {

// Session key management + payload integrity (HMAC-SHA256 style via FNV+xor is NOT crypto).
// We implement a real SHA-256 + HMAC so security is not a decorative stub like Claude's
// secrets.token_hex(32) with no use.

class AuthManager {
public:
    explicit AuthManager(const SecurityConfig& cfg);

    bool initialize();
    void stop();
    [[nodiscard]] bool active() const { return active_; }

    // Rotate session key
    void rotate_key();

    // Authenticate a simple PIN/password hash comparison (constant-time)
    bool authenticate(const std::string& secret);

    // Sign and verify emergency payloads so they cannot be silently tampered with in-process logs
    [[nodiscard]] std::string sign(const std::string& message) const;
    [[nodiscard]] bool verify(const std::string& message, const std::string& hex_mac) const;

    [[nodiscard]] int failed_attempts() const { return failed_; }
    [[nodiscard]] bool locked() const { return failed_ >= cfg_.max_failed_attempts; }

    // Hex of current session key fingerprint (first 8 bytes of SHA256) — not the key itself
    [[nodiscard]] std::string key_fingerprint() const;

private:
    SecurityConfig cfg_;
    bool active_ = false;
    int failed_ = 0;
    std::array<std::uint8_t, 32> session_key_{};
    std::string enrolled_hash_hex_;  // optional; empty = accept any in demo

    static std::array<std::uint8_t, 32> random_key();
    static std::string to_hex(const std::uint8_t* data, std::size_t n);
};

// Pure functions exported for tests
[[nodiscard]] std::array<std::uint8_t, 32> sha256(const std::uint8_t* data, std::size_t len);
[[nodiscard]] std::array<std::uint8_t, 32> hmac_sha256(const std::uint8_t* key, std::size_t key_len,
                                                       const std::uint8_t* msg, std::size_t msg_len);

}  // namespace vg
