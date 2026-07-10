#include "moex_raw/raw_types.hpp"
#include "moex_raw/endian.hpp"
#include "moex_raw/strings.hpp"
#include "moex_raw/crc32c.hpp"
#include "moex_raw/sha256.hpp"
#include <cstring>
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace moex_raw {

// --- Helpers ---

static bool is_all_zero(const std::uint8_t* data, std::size_t len) {
    for (std::size_t i = 0; i < len; ++i) {
        if (data[i] != 0) return false;
    }
    return true;
}

static void add_issue(std::vector<RawValidationIssue>& issues,
                      ValidationSeverity sev, const std::string& code,
                      const std::string& msg) {
    issues.push_back({sev, code, msg});
}

std::string session_id_hex(const std::uint8_t id[16]) {
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (int i = 0; i < 16; ++i) oss << std::setw(2) << static_cast<int>(id[i]);
    return oss.str();
}

bool parse_session_id_hex(const std::string& hex, std::uint8_t id[16]) {
    if (hex.size() != 32) return false;
    for (int i = 0; i < 16; ++i) {
        auto byte_str = hex.substr(i * 2, 2);
        char* end = nullptr;
        auto val = std::strtoul(byte_str.c_str(), &end, 16);
        if (end != byte_str.c_str() + 2) return false;
        id[i] = static_cast<std::uint8_t>(val);
    }
    return true;
}

std::string source_id_hex(std::uint64_t id) {
    std::ostringstream oss;
    oss << std::hex << std::setfill('0') << std::setw(16) << id;
    return oss.str();
}

std::string channel_id_hex(std::uint64_t id) {
    return source_id_hex(id);
}

std::string segment_index_hex(std::uint64_t idx) {
    return source_id_hex(idx);
}

std::string canonical_filename(const std::uint8_t session_id[16], std::uint64_t source_id,
                               std::uint64_t channel_id, std::uint64_t segment_index) {
    return session_id_hex(session_id) + "_src" + source_id_hex(source_id) +
           "_ch" + channel_id_hex(channel_id) + "_seg" + segment_index_hex(segment_index) + ".mxraw";
}

bool parse_segment_index_from_filename(const std::string& filename, std::uint64_t& segment_index) {
    ParsedFilename parsed;
    if (!parse_canonical_filename(filename, parsed)) return false;
    segment_index = parsed.segment_index;
    return true;
}

bool parse_canonical_filename(const std::string& filename, ParsedFilename& parsed) {
    // Expected: <session-32hex>_src<source-16hex>_ch<channel-16hex>_seg<index-16hex>.mxraw
    // Positions: 0-31 session | 32 _ | 33-35 src | 36-51 source | 52 _ | 53-54 ch | 55-70 channel | 71 _ | 72-74 seg | 75-90 index | 91-96 .mxraw
    // Total length: 97
    if (filename.size() != 97) return false;
    if (filename.substr(91) != ".mxraw") return false;

    // Check separators
    if (filename[32] != '_') return false;
    if (filename.substr(33, 3) != "src") return false;
    if (filename[52] != '_') return false;
    if (filename.substr(53, 2) != "ch") return false;
    if (filename[71] != '_') return false;
    if (filename.substr(72, 3) != "seg") return false;

    // Parse session_id (32 hex chars)
    if (!parse_session_id_hex(filename.substr(0, 32), parsed.session_id)) return false;

    // Parse source_id (16 hex chars)
    auto src_hex = filename.substr(36, 16);
    char* end = nullptr;
    parsed.source_id = std::strtoull(src_hex.c_str(), &end, 16);
    if (end != src_hex.c_str() + 16) return false;

    // Parse channel_id (16 hex chars)
    auto ch_hex = filename.substr(55, 16);
    end = nullptr;
    parsed.channel_id = std::strtoull(ch_hex.c_str(), &end, 16);
    if (end != ch_hex.c_str() + 16) return false;

    // Parse segment_index (16 hex chars)
    auto seg_hex = filename.substr(75, 16);
    end = nullptr;
    parsed.segment_index = std::strtoull(seg_hex.c_str(), &end, 16);
    if (end != seg_hex.c_str() + 16) return false;

    return true;
}

std::string sha256_bytes_to_hex(const std::uint8_t hash[32]) {
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (int i = 0; i < 32; ++i) oss << std::setw(2) << static_cast<int>(hash[i]);
    return oss.str();
}

// --- Header serialization ---

void serialize_header(std::vector<std::uint8_t>& buf, const RawSegmentMetadata& meta) {
    // Preamble
    write_bytes(buf, kMagicRaw, 8);
    write_u16_le(buf, kFormatVersion);

    // header_size placeholder — will be filled in after we know the size
    std::size_t header_size_offset = buf.size();
    write_u32_le(buf, 0);  // placeholder
    write_u32_le(buf, 0);  // format_flags = 0

    // Segment metadata
    write_bytes(buf, meta.session.session_id, 16);
    write_u64_le(buf, meta.segment_index);
    write_u64_le(buf, meta.start_capture_index);
    write_u64_le(buf, meta.created_utc_ns);
    buf.push_back(static_cast<std::uint8_t>(meta.source.clock_domain));
    buf.push_back(static_cast<std::uint8_t>(meta.source.transport));
    buf.push_back(static_cast<std::uint8_t>(meta.source.source_side));
    buf.push_back(0);  // reserved
    write_u64_le(buf, meta.source.source_id);
    write_u64_le(buf, meta.source.channel_id);
    write_bytes(buf, meta.source.configuration_sha256, 32);
    write_bytes(buf, meta.source.templates_sha256, 32);
    write_bytes(buf, meta.source.endpoint_fingerprint_sha256, 32);
    if (!write_length_string(buf, meta.session.feed_group)) return;
    if (!write_length_string(buf, meta.session.endpoint_role)) return;
    if (!write_length_string(buf, meta.session.source_label)) return;

    // Patch header_size
    std::uint32_t header_size = static_cast<std::uint32_t>(buf.size());
    buf[header_size_offset]     = static_cast<std::uint8_t>(header_size);
    buf[header_size_offset + 1] = static_cast<std::uint8_t>(header_size >> 8);
    buf[header_size_offset + 2] = static_cast<std::uint8_t>(header_size >> 16);
    buf[header_size_offset + 3] = static_cast<std::uint8_t>(header_size >> 24);
}

bool deserialize_header(const std::uint8_t* data, std::size_t len, RawSegmentMetadata& meta,
                        std::size_t& header_size, std::vector<RawValidationIssue>& issues) {
    // Minimum header: 8(magic) + 2(version) + 4(header_size) + 4(flags) + 16(session) + 8(seg) + 8(start) + 8(created)
    //               + 1(clock) + 1(transport) + 1(side) + 1(reserved) + 8(source) + 8(channel) + 32*3(hashes)
    //               + 2(min string) = at least 126 bytes
    static constexpr std::size_t kMinHeader = 126;
    if (len < kMinHeader) {
        add_issue(issues, ValidationSeverity::Error, "HEADER_TOO_SHORT", "File too short for header");
        return false;
    }

    // Magic
    if (std::memcmp(data, kMagicRaw, 8) != 0) {
        add_issue(issues, ValidationSeverity::Error, "WRONG_MAGIC", "Wrong file magic");
        return false;
    }

    // Format version
    std::uint16_t version = read_u16_le(data + 8);
    if (version != kFormatVersion) {
        add_issue(issues, ValidationSeverity::Error, "UNSUPPORTED_VERSION", "Unsupported format version");
        return false;
    }

    // Header size
    header_size = read_u32_le(data + 10);
    if (header_size > kMaxHeaderSize || header_size > len) {
        add_issue(issues, ValidationSeverity::Error, "INVALID_HEADER_SIZE", "Invalid header size");
        return false;
    }

    // Format flags
    std::uint32_t format_flags = read_u32_le(data + 14);
    if (format_flags != 0) {
        add_issue(issues, ValidationSeverity::Error, "NONZERO_FORMAT_FLAGS", "Non-zero format flags in v1");
        return false;
    }

    const std::uint8_t* p = data + 18;
    std::size_t remaining = header_size - 18;

    // session_id (16)
    if (remaining < 16) { add_issue(issues, ValidationSeverity::Error, "TRUNCATED_HEADER", "Truncated header"); return false; }
    std::memcpy(meta.session.session_id, p, 16);
    if (is_all_zero(meta.session.session_id, 16)) {
        add_issue(issues, ValidationSeverity::Error, "ZERO_SESSION_ID", "All-zero session ID");
        return false;
    }
    p += 16; remaining -= 16;

    // segment_index (8), start_capture_index (8), created_utc_ns (8)
    if (remaining < 24) { add_issue(issues, ValidationSeverity::Error, "TRUNCATED_HEADER", "Truncated header"); return false; }
    meta.segment_index = read_u64_le(p); p += 8;
    meta.start_capture_index = read_u64_le(p); p += 8;
    meta.created_utc_ns = read_u64_le(p); p += 8;
    remaining -= 24;

    if (meta.created_utc_ns == 0) {
        add_issue(issues, ValidationSeverity::Error, "ZERO_CREATED_UTC", "Zero created_utc_ns");
        return false;
    }

    // clock_domain (1), transport (1), source_side (1), reserved (1)
    if (remaining < 4) { add_issue(issues, ValidationSeverity::Error, "TRUNCATED_HEADER", "Truncated header"); return false; }
    auto clock_val = p[0];
    auto transport_val = p[1];
    auto side_val = p[2];
    auto reserved_val = p[3];

    if (clock_val < 1 || clock_val > 3) {
        add_issue(issues, ValidationSeverity::Error, "UNSUPPORTED_ENUM", "Unsupported clock_domain value");
        return false;
    }
    meta.source.clock_domain = static_cast<ClockDomain>(clock_val);

    if (transport_val > 2) {
        add_issue(issues, ValidationSeverity::Error, "UNSUPPORTED_ENUM", "Unsupported transport value");
        return false;
    }
    meta.source.transport = static_cast<Transport>(transport_val);

    if (side_val > 2) {
        add_issue(issues, ValidationSeverity::Error, "UNSUPPORTED_ENUM", "Unsupported source_side value");
        return false;
    }
    meta.source.source_side = static_cast<SourceSide>(side_val);

    if (reserved_val != 0) {
        add_issue(issues, ValidationSeverity::Error, "NONZERO_RESERVED", "Non-zero reserved byte");
        return false;
    }
    p += 4; remaining -= 4;

    // source_id (8), channel_id (8)
    if (remaining < 16) { add_issue(issues, ValidationSeverity::Error, "TRUNCATED_HEADER", "Truncated header"); return false; }
    meta.source.source_id = read_u64_le(p); p += 8;
    meta.source.channel_id = read_u64_le(p); p += 8;
    remaining -= 16;

    if (meta.source.source_id == 0) {
        add_issue(issues, ValidationSeverity::Error, "ZERO_SOURCE_ID", "Zero source_id");
        return false;
    }
    if (meta.source.channel_id == 0) {
        add_issue(issues, ValidationSeverity::Error, "ZERO_CHANNEL_ID", "Zero channel_id");
        return false;
    }

    // Three SHA-256 values (32 each)
    if (remaining < 96) { add_issue(issues, ValidationSeverity::Error, "TRUNCATED_HEADER", "Truncated header"); return false; }
    std::memcpy(meta.source.configuration_sha256, p, 32);
    std::memcpy(meta.source.templates_sha256, p + 32, 32);
    std::memcpy(meta.source.endpoint_fingerprint_sha256, p + 64, 32);

    if (is_all_zero(meta.source.configuration_sha256, 32) ||
        is_all_zero(meta.source.templates_sha256, 32) ||
        is_all_zero(meta.source.endpoint_fingerprint_sha256, 32)) {
        add_issue(issues, ValidationSeverity::Error, "ZERO_HASH", "All-zero SHA-256 hash");
        return false;
    }
    p += 96; remaining -= 96;

    // Strings: feed_group, endpoint_role, source_label
    std::size_t consumed = 0;
    if (!read_length_string(p, remaining, meta.session.feed_group, consumed)) {
        add_issue(issues, ValidationSeverity::Error, "INVALID_STRING", "Invalid feed_group string");
        return false;
    }
    p += consumed; remaining -= consumed;

    if (meta.session.feed_group.empty()) {
        add_issue(issues, ValidationSeverity::Error, "EMPTY_FEED_GROUP", "Empty feed_group");
        return false;
    }

    if (!read_length_string(p, remaining, meta.session.endpoint_role, consumed)) {
        add_issue(issues, ValidationSeverity::Error, "INVALID_STRING", "Invalid endpoint_role string");
        return false;
    }
    p += consumed; remaining -= consumed;

    if (meta.session.endpoint_role.empty()) {
        add_issue(issues, ValidationSeverity::Error, "EMPTY_ENDPOINT_ROLE", "Empty endpoint_role");
        return false;
    }

    if (!read_length_string(p, remaining, meta.session.source_label, consumed)) {
        add_issue(issues, ValidationSeverity::Error, "INVALID_STRING", "Invalid source_label string");
        return false;
    }
    p += consumed;

    return true;
}

// --- Record serialization ---

void serialize_record(std::vector<std::uint8_t>& buf, const RawPacketRecord& rec) {
    std::uint32_t payload_size = static_cast<std::uint32_t>(rec.payload.size());
    std::uint32_t record_size = kRecordHeaderSize + payload_size + 4;

    // record_magic
    write_bytes(buf, kMagicRec, 4);
    write_u16_le(buf, kRecordHeaderSize);
    write_u16_le(buf, rec.record_flags);
    write_u32_le(buf, record_size);
    write_u64_le(buf, rec.capture_index);
    write_u64_le(buf, rec.capture_utc_ns);
    write_u64_le(buf, rec.capture_monotonic_ns);
    write_u32_le(buf, payload_size);

    // payload CRC32C
    std::uint32_t payload_crc = 0;
    if (!rec.payload.empty()) {
        payload_crc = crc32c(rec.payload.data(), rec.payload.size());
    }
    write_u32_le(buf, payload_crc);

    // payload
    write_bytes(buf, rec.payload.data(), rec.payload.size());

    // record CRC32C over all bytes before this field
    std::uint32_t record_crc = crc32c(buf.data() + buf.size() - (kRecordHeaderSize - 4 + payload_size + 4),
                                       kRecordHeaderSize - 4 + payload_size + 4);
    // Actually, record CRC is over ALL record bytes before the final field.
    // The record bytes are: magic(4) + header_size(2) + flags(2) + record_size(4) + capture_index(8)
    //   + capture_utc_ns(8) + capture_monotonic_ns(8) + payload_size(4) + payload_crc32c(4) + payload
    // So record_crc covers everything from magic through payload.
    std::size_t record_data_start = buf.size() - (kRecordHeaderSize - 4 + payload_size + 4);
    std::size_t record_data_len = kRecordHeaderSize - 4 + payload_size + 4;
    record_crc = crc32c(buf.data() + record_data_start, record_data_len);
    write_u32_le(buf, record_crc);
}

bool deserialize_record_header(const std::uint8_t* data, std::size_t available,
                               RawPacketRecord& rec, std::size_t& record_total_size,
                               std::vector<RawValidationIssue>& issues) {
    // Minimum: 44 bytes for record header
    if (available < kRecordHeaderSize) {
        add_issue(issues, ValidationSeverity::Error, "TRUNCATED_RECORD", "Truncated record header");
        return false;
    }

    // record_magic
    if (std::memcmp(data, kMagicRec, 4) != 0) {
        add_issue(issues, ValidationSeverity::Error, "WRONG_RECORD_MAGIC", "Wrong record magic");
        return false;
    }

    // record_header_size
    std::uint16_t header_size = read_u16_le(data + 4);
    if (header_size != kRecordHeaderSize) {
        add_issue(issues, ValidationSeverity::Error, "WRONG_RECORD_HEADER_SIZE", "Record header size not 44");
        return false;
    }

    // record_flags
    rec.record_flags = read_u16_le(data + 6);
    if (rec.record_flags & ~kRecordFlagUtcValid) {
        add_issue(issues, ValidationSeverity::Error, "UNKNOWN_RECORD_FLAG", "Unknown record flag bit");
        return false;
    }

    // record_size
    std::uint32_t record_size = read_u32_le(data + 8);
    std::uint32_t payload_size = read_u32_le(data + 36);

    // Validate payload_size
    if (payload_size > kMaxPayloadSize) {
        add_issue(issues, ValidationSeverity::Error, "PAYLOAD_TOO_LARGE", "Payload exceeds 1 MiB");
        return false;
    }

    // Validate record_size = 44 + payload_size + 4
    std::uint32_t expected_record_size = kRecordHeaderSize + payload_size + 4;
    if (record_size != expected_record_size) {
        add_issue(issues, ValidationSeverity::Error, "WRONG_RECORD_SIZE", "record_size mismatch");
        return false;
    }

    record_total_size = record_size;

    // Check if we have enough data
    if (available < record_size) {
        add_issue(issues, ValidationSeverity::Error, "TRUNCATED_RECORD", "Truncated record (payload or checksum)");
        return false;
    }

    rec.capture_index = read_u64_le(data + 12);
    rec.capture_utc_ns = read_u64_le(data + 20);
    rec.capture_monotonic_ns = read_u64_le(data + 28);

    // payload CRC32C
    std::uint32_t stored_payload_crc = read_u32_le(data + 40);
    const std::uint8_t* payload_data = data + kRecordHeaderSize;

    if (payload_size == 0) {
        if (stored_payload_crc != 0) {
            add_issue(issues, ValidationSeverity::Error, "WRONG_PAYLOAD_CRC", "Zero-length payload but non-zero CRC");
            return false;
        }
        rec.payload.clear();
    } else {
        std::uint32_t computed_payload_crc = crc32c(payload_data, payload_size);
        if (stored_payload_crc != computed_payload_crc) {
            add_issue(issues, ValidationSeverity::Error, "WRONG_PAYLOAD_CRC", "Payload CRC32C mismatch");
            return false;
        }
        rec.payload.assign(payload_data, payload_data + payload_size);
    }

    // record CRC32C — over all record bytes before the final 4-byte field
    std::uint32_t stored_record_crc = read_u32_le(data + record_size - 4);
    std::uint32_t computed_record_crc = crc32c(data, record_size - 4);
    if (stored_record_crc != computed_record_crc) {
        add_issue(issues, ValidationSeverity::Error, "WRONG_RECORD_CRC", "Record CRC32C mismatch");
        return false;
    }

    return true;
}

// --- Footer serialization ---

void serialize_footer(std::vector<std::uint8_t>& buf, const RawFooter& footer) {
    // Magic
    write_bytes(buf, kMagicEnd, 8);
    write_u32_le(buf, kFooterSize);   // footer_size
    write_u32_le(buf, 0);             // footer_flags = 0
    write_u64_le(buf, footer.record_count);
    write_u64_le(buf, footer.first_capture_index);
    write_u64_le(buf, footer.last_capture_index);
    write_u64_le(buf, footer.total_payload_bytes);
    write_u64_le(buf, footer.data_bytes_before_footer);
    write_bytes(buf, footer.content_sha256, 32);

    // footer CRC32C over first 88 bytes
    std::uint32_t footer_crc = crc32c(buf.data() + buf.size() - 88, 88);
    write_u32_le(buf, footer_crc);
}

bool deserialize_footer(const std::uint8_t* data, std::size_t available,
                        RawFooter& footer, std::vector<RawValidationIssue>& issues) {
    if (available < kFooterSize) {
        add_issue(issues, ValidationSeverity::Error, "TRUNCATED_FOOTER", "Truncated or missing footer");
        return false;
    }

    // Magic
    if (std::memcmp(data, kMagicEnd, 8) != 0) {
        add_issue(issues, ValidationSeverity::Error, "WRONG_FOOTER_MAGIC", "Wrong footer magic");
        return false;
    }

    // footer_size
    std::uint32_t footer_size = read_u32_le(data + 8);
    if (footer_size != kFooterSize) {
        add_issue(issues, ValidationSeverity::Error, "WRONG_FOOTER_SIZE", "Footer size not 92");
        return false;
    }

    // footer_flags
    std::uint32_t footer_flags = read_u32_le(data + 12);
    if (footer_flags != 0) {
        add_issue(issues, ValidationSeverity::Error, "NONZERO_FOOTER_FLAGS", "Non-zero footer flags");
        return false;
    }

    footer.record_count = read_u64_le(data + 16);
    footer.first_capture_index = read_u64_le(data + 24);
    footer.last_capture_index = read_u64_le(data + 32);
    footer.total_payload_bytes = read_u64_le(data + 40);
    footer.data_bytes_before_footer = read_u64_le(data + 48);
    std::memcpy(footer.content_sha256, data + 56, 32);

    // footer CRC32C over first 88 bytes
    std::uint32_t stored_crc = read_u32_le(data + 88);
    std::uint32_t computed_crc = crc32c(data, 88);
    if (stored_crc != computed_crc) {
        add_issue(issues, ValidationSeverity::Error, "WRONG_FOOTER_CRC", "Footer CRC32C mismatch");
        return false;
    }

    return true;
}

}  // namespace moex_raw
