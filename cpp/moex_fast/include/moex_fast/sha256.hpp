#pragma once
#include <cstddef>
#include <cstdint>
#include <string>

namespace moex_fast {

// Pure C++ SHA-256 implementation. Returns lowercase hex digest, or empty on error.
std::string compute_sha256_file(const std::string& path);

}  // namespace moex_fast
