#include "visionglove/security.hpp"
#include "visionglove/logger.hpp"

#include <cstring>
#include <random>
#include <sstream>
#include <iomanip>

namespace vg {
namespace {

// Full SHA-256 (public domain style compact implementation)
class Sha256 {
public:
    Sha256() { reset(); }

    void update(const std::uint8_t* data, std::size_t len) {
        for (std::size_t i = 0; i < len; ++i) {
            data_[datalen_++] = data[i];
            if (datalen_ == 64) {
                transform();
                bitlen_ += 512;
                datalen_ = 0;
            }
        }
    }

    std::array<std::uint8_t, 32> digest() {
        std::array<std::uint8_t, 32> hash{};
        pad();
        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 8; ++j) {
                hash[i + j * 4] = static_cast<std::uint8_t>((state_[j] >> (24 - i * 8)) & 0xff);
            }
        }
        return hash;
    }

private:
    void reset() {
        datalen_ = 0;
        bitlen_ = 0;
        state_[0] = 0x6a09e667;
        state_[1] = 0xbb67ae85;
        state_[2] = 0x3c6ef372;
        state_[3] = 0xa54ff53a;
        state_[4] = 0x510e527f;
        state_[5] = 0x9b05688c;
        state_[6] = 0x1f83d9ab;
        state_[7] = 0x5be0cd19;
    }

    static std::uint32_t rotr(std::uint32_t x, std::uint32_t n) {
        return (x >> n) | (x << (32 - n));
    }

    void transform() {
        static constexpr std::uint32_t k[64] = {
            0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
            0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
            0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
            0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
            0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
            0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
            0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
            0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
        };
        std::uint32_t m[64];
        for (int i = 0, j = 0; i < 16; ++i, j += 4) {
            m[i] = (static_cast<std::uint32_t>(data_[j]) << 24) |
                   (static_cast<std::uint32_t>(data_[j + 1]) << 16) |
                   (static_cast<std::uint32_t>(data_[j + 2]) << 8) |
                   (static_cast<std::uint32_t>(data_[j + 3]));
        }
        for (int i = 16; i < 64; ++i) {
            const std::uint32_t s0 = rotr(m[i - 15], 7) ^ rotr(m[i - 15], 18) ^ (m[i - 15] >> 3);
            const std::uint32_t s1 = rotr(m[i - 2], 17) ^ rotr(m[i - 2], 19) ^ (m[i - 2] >> 10);
            m[i] = m[i - 16] + s0 + m[i - 7] + s1;
        }
        std::uint32_t a = state_[0], b = state_[1], c = state_[2], d = state_[3];
        std::uint32_t e = state_[4], f = state_[5], g = state_[6], h = state_[7];
        for (int i = 0; i < 64; ++i) {
            const std::uint32_t S1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
            const std::uint32_t ch = (e & f) ^ ((~e) & g);
            const std::uint32_t t1 = h + S1 + ch + k[i] + m[i];
            const std::uint32_t S0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
            const std::uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
            const std::uint32_t t2 = S0 + maj;
            h = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
        }
        state_[0] += a; state_[1] += b; state_[2] += c; state_[3] += d;
        state_[4] += e; state_[5] += f; state_[6] += g; state_[7] += h;
    }

    void pad() {
        std::size_t i = datalen_;
        if (datalen_ < 56) {
            data_[i++] = 0x80;
            while (i < 56) data_[i++] = 0x00;
        } else {
            data_[i++] = 0x80;
            while (i < 64) data_[i++] = 0x00;
            transform();
            std::memset(data_, 0, 56);
        }
        bitlen_ += datalen_ * 8;
        data_[63] = static_cast<std::uint8_t>(bitlen_);
        data_[62] = static_cast<std::uint8_t>(bitlen_ >> 8);
        data_[61] = static_cast<std::uint8_t>(bitlen_ >> 16);
        data_[60] = static_cast<std::uint8_t>(bitlen_ >> 24);
        data_[59] = static_cast<std::uint8_t>(bitlen_ >> 32);
        data_[58] = static_cast<std::uint8_t>(bitlen_ >> 40);
        data_[57] = static_cast<std::uint8_t>(bitlen_ >> 48);
        data_[56] = static_cast<std::uint8_t>(bitlen_ >> 56);
        transform();
    }

    std::uint8_t data_[64]{};
    std::uint32_t datalen_ = 0;
    std::uint64_t bitlen_ = 0;
    std::uint32_t state_[8]{};
};

bool const_time_eq(const std::uint8_t* a, const std::uint8_t* b, std::size_t n) {
    std::uint8_t diff = 0;
    for (std::size_t i = 0; i < n; ++i) diff |= static_cast<std::uint8_t>(a[i] ^ b[i]);
    return diff == 0;
}

}  // namespace

std::array<std::uint8_t, 32> sha256(const std::uint8_t* data, std::size_t len) {
    Sha256 s;
    s.update(data, len);
    return s.digest();
}

std::array<std::uint8_t, 32> hmac_sha256(const std::uint8_t* key, std::size_t key_len,
                                         const std::uint8_t* msg, std::size_t msg_len) {
    std::array<std::uint8_t, 64> k0{};
    if (key_len > 64) {
        auto hk = sha256(key, key_len);
        std::memcpy(k0.data(), hk.data(), 32);
    } else {
        std::memcpy(k0.data(), key, key_len);
    }
    std::array<std::uint8_t, 64> ipad{}, opad{};
    for (int i = 0; i < 64; ++i) {
        ipad[i] = static_cast<std::uint8_t>(k0[i] ^ 0x36);
        opad[i] = static_cast<std::uint8_t>(k0[i] ^ 0x5c);
    }
    // inner
    Sha256 inner;
    inner.update(ipad.data(), 64);
    inner.update(msg, msg_len);
    auto inner_hash = inner.digest();
    // outer
    Sha256 outer;
    outer.update(opad.data(), 64);
    outer.update(inner_hash.data(), 32);
    return outer.digest();
}

AuthManager::AuthManager(const SecurityConfig& cfg) : cfg_(cfg) {}

std::array<std::uint8_t, 32> AuthManager::random_key() {
    std::array<std::uint8_t, 32> key{};
    std::random_device rd;
    for (auto& b : key) b = static_cast<std::uint8_t>(rd());
    return key;
}

std::string AuthManager::to_hex(const std::uint8_t* data, std::size_t n) {
    std::ostringstream oss;
    for (std::size_t i = 0; i < n; ++i)
        oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(data[i]);
    return oss.str();
}

bool AuthManager::initialize() {
    if (cfg_.encryption_enabled) {
        session_key_ = random_key();
    }
    active_ = true;
    VG_LOG_INFO("Auth", "Authentication manager initialized (key fingerprint " + key_fingerprint() + ")");
    return true;
}

void AuthManager::stop() {
    active_ = false;
    session_key_.fill(0);
}

void AuthManager::rotate_key() {
    session_key_ = random_key();
    VG_LOG_INFO("Auth", "Session key rotated (" + key_fingerprint() + ")");
}

bool AuthManager::authenticate(const std::string& secret) {
    if (locked()) {
        VG_LOG_WARN("Auth", "Account locked due to failed attempts");
        return false;
    }
    // Demo mode: empty enrolled hash accepts non-empty secrets of length >= 4
    if (enrolled_hash_hex_.empty()) {
        if (secret.size() >= 4) {
            failed_ = 0;
            return true;
        }
        ++failed_;
        return false;
    }
    auto h = sha256(reinterpret_cast<const std::uint8_t*>(secret.data()), secret.size());
    auto hex = to_hex(h.data(), h.size());
    if (hex == enrolled_hash_hex_) {
        failed_ = 0;
        return true;
    }
    ++failed_;
    return false;
}

std::string AuthManager::sign(const std::string& message) const {
    auto mac = hmac_sha256(session_key_.data(), session_key_.size(),
                           reinterpret_cast<const std::uint8_t*>(message.data()), message.size());
    return to_hex(mac.data(), mac.size());
}

bool AuthManager::verify(const std::string& message, const std::string& hex_mac) const {
    auto expected = sign(message);
    if (expected.size() != hex_mac.size()) return false;
    return const_time_eq(reinterpret_cast<const std::uint8_t*>(expected.data()),
                         reinterpret_cast<const std::uint8_t*>(hex_mac.data()),
                         expected.size());
}

std::string AuthManager::key_fingerprint() const {
    auto fp = sha256(session_key_.data(), session_key_.size());
    return to_hex(fp.data(), 8);
}

}  // namespace vg
