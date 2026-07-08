#include "qsh/qsh_reader.hpp"
#include "qsh/sha256.hpp"
#include "qsh/leb128.hpp"
#include <zlib.h>
#include <fstream>

namespace qsh {

QshFile open_qsh_file(const std::string& path) {
    QshFile result;

    // Read entire file
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        result.error = "Cannot open file: " + path;
        return result;
    }

    auto file_size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<uint8_t> raw(static_cast<size_t>(file_size));
    file.read(reinterpret_cast<char*>(raw.data()), file_size);
    file.close();

    if (raw.size() < kQshSignatureLen + 1) {
        result.error = "File too small";
        return result;
    }

    // QSH files are gzip-compressed. Check for gzip magic bytes.
    bool is_gzip = (raw.size() >= 2 && raw[0] == 0x1f && raw[1] == 0x8b);

    std::vector<uint8_t> decompressed;

    if (is_gzip) {
        z_stream zs{};
        if (inflateInit2(&zs, 15 + 32) != Z_OK) {
            result.error = "zlib inflateInit2 failed";
            return result;
        }

        zs.next_in = raw.data();
        zs.avail_in = static_cast<uInt>(raw.size());

        constexpr size_t kChunkSize = 65536;
        decompressed.reserve(raw.size() * 4);

        int ret;
        do {
            size_t offset = decompressed.size();
            decompressed.resize(offset + kChunkSize);
            zs.next_out = decompressed.data() + offset;
            zs.avail_out = static_cast<uInt>(kChunkSize);

            ret = inflate(&zs, Z_NO_FLUSH);
            if (ret != Z_OK && ret != Z_STREAM_END && ret != Z_BUF_ERROR) {
                inflateEnd(&zs);
                result.error = "zlib inflate error: " + std::string(zs.msg ? zs.msg : "unknown");
                return result;
            }

            decompressed.resize(offset + kChunkSize - zs.avail_out);
        } while (ret != Z_STREAM_END);

        inflateEnd(&zs);
    } else {
        decompressed = std::move(raw);
    }

    // Parse header
    auto header_result = parse_qsh_header(decompressed.data(), decompressed.size());
    if (!header_result.valid) {
        result.error = header_result.error;
        return result;
    }

    result.header = header_result.header;

    // Re-parse header to find exact end offset
    size_t offset = kQshSignatureLen + 1;  // signature + version
    try {
        // Skip recorder
        int64_t len = read_leb128(decompressed.data(), decompressed.size(), offset);
        offset += static_cast<size_t>(len);
        // Skip comment
        len = read_leb128(decompressed.data(), decompressed.size(), offset);
        offset += static_cast<size_t>(len);
        // Skip recording_time (8) + stream_count (1) + stream_type (1)
        offset += 8 + 1 + 1;
        // Skip instrument
        len = read_leb128(decompressed.data(), decompressed.size(), offset);
        offset += static_cast<size_t>(len);
    } catch (const std::exception& e) {
        result.error = std::string("Header re-parse error: ") + e.what();
        return result;
    }

    // Data starts after header
    result.data.assign(decompressed.begin() + static_cast<ptrdiff_t>(offset), decompressed.end());
    result.data_offset = 0;
    result.valid = true;
    return result;
}

std::string file_sha256_hex(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) return "";

    Sha256 ctx;
    char buf[8192];
    while (file.read(buf, sizeof(buf))) {
        ctx.update(reinterpret_cast<const uint8_t*>(buf), static_cast<size_t>(file.gcount()));
    }
    if (file.gcount() > 0) {
        ctx.update(reinterpret_cast<const uint8_t*>(buf), static_cast<size_t>(file.gcount()));
    }

    auto hash = ctx.final();
    static const char hex[] = "0123456789abcdef";
    std::string result;
    result.reserve(64);
    for (auto b : hash) {
        result += hex[b >> 4];
        result += hex[b & 0x0f];
    }
    return result;
}

}  // namespace qsh
