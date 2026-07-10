#include "moex_fast/sha256.hpp"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <cstring>

namespace moex_fast {

namespace {

struct SHA256Ctx {
    std::uint32_t state[8];
    std::uint64_t count;
    std::uint8_t buf[64];
};

constexpr std::uint32_t K[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
    0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
    0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
    0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
    0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
    0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
    0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5,
    0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
    0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

std::uint32_t rotr(std::uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }
std::uint32_t ch(std::uint32_t x, std::uint32_t y, std::uint32_t z) { return (x & y) ^ (~x & z); }
std::uint32_t maj(std::uint32_t x, std::uint32_t y, std::uint32_t z) { return (x & y) ^ (x & z) ^ (y & z); }
std::uint32_t sigma0(std::uint32_t x) { return rotr(x, 2) ^ rotr(x, 13) ^ rotr(x, 22); }
std::uint32_t sigma1(std::uint32_t x) { return rotr(x, 6) ^ rotr(x, 11) ^ rotr(x, 25); }
std::uint32_t gamma0(std::uint32_t x) { return rotr(x, 7) ^ rotr(x, 18) ^ (x >> 3); }
std::uint32_t gamma1(std::uint32_t x) { return rotr(x, 17) ^ rotr(x, 19) ^ (x >> 10); }

void sha256_transform(SHA256Ctx& ctx, const std::uint8_t block[64]) {
    std::uint32_t W[64];
    for (int i = 0; i < 16; ++i) {
        W[i] = (static_cast<std::uint32_t>(block[i * 4]) << 24) |
               (static_cast<std::uint32_t>(block[i * 4 + 1]) << 16) |
               (static_cast<std::uint32_t>(block[i * 4 + 2]) << 8) |
               (static_cast<std::uint32_t>(block[i * 4 + 3]));
    }
    for (int i = 16; i < 64; ++i) {
        W[i] = gamma1(W[i - 2]) + W[i - 7] + gamma0(W[i - 15]) + W[i - 16];
    }

    std::uint32_t a = ctx.state[0], b = ctx.state[1];
    std::uint32_t c = ctx.state[2], d = ctx.state[3];
    std::uint32_t e = ctx.state[4], f = ctx.state[5];
    std::uint32_t g = ctx.state[6], h = ctx.state[7];

    for (int i = 0; i < 64; ++i) {
        std::uint32_t T1 = h + sigma1(e) + ch(e, f, g) + K[i] + W[i];
        std::uint32_t T2 = sigma0(a) + maj(a, b, c);
        h = g; g = f; f = e; e = d + T1;
        d = c; c = b; b = a; a = T1 + T2;
    }

    ctx.state[0] += a; ctx.state[1] += b;
    ctx.state[2] += c; ctx.state[3] += d;
    ctx.state[4] += e; ctx.state[5] += f;
    ctx.state[6] += g; ctx.state[7] += h;
}

void sha256_init(SHA256Ctx& ctx) {
    ctx.state[0] = 0x6a09e667; ctx.state[1] = 0xbb67ae85;
    ctx.state[2] = 0x3c6ef372; ctx.state[3] = 0xa54ff53a;
    ctx.state[4] = 0x510e527f; ctx.state[5] = 0x9b05688c;
    ctx.state[6] = 0x1f83d9ab; ctx.state[7] = 0x5be0cd19;
    ctx.count = 0;
    std::memset(ctx.buf, 0, 64);
}

void sha256_update(SHA256Ctx& ctx, const std::uint8_t* data, std::size_t len) {
    std::size_t idx = static_cast<std::size_t>(ctx.count & 63);
    ctx.count += len;

    if (idx) {
        std::size_t part = 64 - idx;
        if (len >= part) {
            std::memcpy(ctx.buf + idx, data, part);
            sha256_transform(ctx, ctx.buf);
            data += part;
            len -= part;
            idx = 0;
        } else {
            std::memcpy(ctx.buf + idx, data, len);
            return;
        }
    }

    while (len >= 64) {
        sha256_transform(ctx, data);
        data += 64;
        len -= 64;
    }

    if (len) {
        std::memcpy(ctx.buf + idx, data, len);
    }
}

void sha256_final(SHA256Ctx& ctx, std::uint8_t hash[32]) {
    std::size_t idx = static_cast<std::size_t>(ctx.count & 63);
    ctx.buf[idx++] = 0x80;

    if (idx > 56) {
        std::memset(ctx.buf + idx, 0, 64 - idx);
        sha256_transform(ctx, ctx.buf);
        idx = 0;
    }
    std::memset(ctx.buf + idx, 0, 56 - idx);

    std::uint64_t bits = ctx.count * 8;
    for (int i = 0; i < 8; ++i) {
        ctx.buf[56 + i] = static_cast<std::uint8_t>(bits >> (56 - i * 8));
    }
    sha256_transform(ctx, ctx.buf);

    for (int i = 0; i < 8; ++i) {
        hash[i * 4]     = static_cast<std::uint8_t>(ctx.state[i] >> 24);
        hash[i * 4 + 1] = static_cast<std::uint8_t>(ctx.state[i] >> 16);
        hash[i * 4 + 2] = static_cast<std::uint8_t>(ctx.state[i] >> 8);
        hash[i * 4 + 3] = static_cast<std::uint8_t>(ctx.state[i]);
    }
}

}  // namespace

std::string compute_sha256_file(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) return {};

    SHA256Ctx ctx;
    sha256_init(ctx);

    char buf[4096];
    while (file.read(buf, sizeof(buf)) || file.gcount() > 0) {
        sha256_update(ctx, reinterpret_cast<const std::uint8_t*>(buf),
                      static_cast<std::size_t>(file.gcount()));
    }

    std::uint8_t hash[32];
    sha256_final(ctx, hash);

    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (int i = 0; i < 32; ++i) {
        oss << std::setw(2) << static_cast<int>(hash[i]);
    }
    return oss.str();
}

}  // namespace moex_fast
