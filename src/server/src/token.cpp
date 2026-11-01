// token.cpp -- 128-bit per-launch token generation.
#include "workbench/token.hpp"
#include <algorithm>
#include <array>
#include <random>
#include <sstream>
#include <iomanip>

namespace workbench {

std::string generate_token() {
    // Two 64-bit halves from random_device-seeded MT engine = 128 bits.
    std::random_device rd;
    std::mt19937_64 eng(
        (static_cast<uint64_t>(rd()) << 32) | static_cast<uint64_t>(rd())
    );
    std::uniform_int_distribution<uint64_t> dist;
    const uint64_t hi = dist(eng);
    const uint64_t lo = dist(eng);
    std::ostringstream oss;
    oss << std::hex << std::setfill('0')
        << std::setw(16) << hi
        << std::setw(16) << lo;
    return oss.str();
}

bool token_well_formed(const std::string& token) noexcept {
    if (token.size() != 32) return false;
    return std::all_of(token.begin(), token.end(), [](char c) {
        return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
    });
}

}  // namespace workbench
