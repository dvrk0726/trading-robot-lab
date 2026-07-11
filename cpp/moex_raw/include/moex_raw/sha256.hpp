#pragma once
#include <cstddef>
#include <cstdint>
#include <string>

namespace moex_raw {

// Streaming SHA-256 context for incremental hashing.
struct SHA256Ctx {
    std::uint32_t state[8];
    std::uint64_t count;
    std::uint8_t buf[64];
};

void sha256_init(SHA256Ctx& ctx);
void sha256_update(SHA256Ctx& ctx, const std::uint8_t* data, std::size_t len);
void sha256_final(SHA256Ctx& ctx, std::uint8_t hash[32]);

// One-shot helpers.
void sha256(const void* data, std::size_t len, std::uint8_t hash[32]);
std::string sha256_hex(const void* data, std::size_t len);
std::string sha256_hex_file(const std::string& path);

}  // namespace moex_raw
