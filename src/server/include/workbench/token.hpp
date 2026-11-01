#pragma once
// token.hpp — per-launch 128-bit random token, hex-encoded.
// The token is generated once at process start using a cryptographically
// adequate source (std::random_device seeding a 64-bit MT engine twice).
// Length is always 32 hex characters (128 bits).
#include <string>

namespace workbench {

// Generate a fresh 128-bit token encoded as 32 lowercase hex characters.
// Precondition: called at most once per process (cheap; no state stored here).
[[nodiscard]] std::string generate_token();

// Return true iff token is exactly 32 lowercase hex characters.
[[nodiscard]] bool token_well_formed(const std::string& token) noexcept;

}  // namespace workbench