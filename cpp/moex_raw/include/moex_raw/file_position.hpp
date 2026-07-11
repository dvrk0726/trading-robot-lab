#pragma once
#include <cstdint>
#include <cstdio>

#ifdef _WIN32
#include <io.h>
#endif

namespace moex_raw {

// Portable 64-bit file seek. Returns 0 on success.
inline int fseek64(std::FILE* f, std::int64_t offset, int origin) {
#ifdef _WIN32
    return _fseeki64(f, offset, origin);
#else
    return std::fseek(f, static_cast<long>(offset), origin);
#endif
}

// Portable 64-bit file tell. Returns -1 on error.
inline std::int64_t ftell64(std::FILE* f) {
#ifdef _WIN32
    return _ftelli64(f);
#else
    return static_cast<std::int64_t>(std::ftell(f));
#endif
}

}  // namespace moex_raw
