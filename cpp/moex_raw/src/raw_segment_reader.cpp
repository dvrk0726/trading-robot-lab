#include "moex_raw/raw_segment.hpp"
#include "moex_raw/raw_types.hpp"
#include "moex_raw/endian.hpp"
#include "moex_raw/crc32c.hpp"
#include "moex_raw/sha256.hpp"
#include <cstdio>
#include <filesystem>
#include <cstring>
#include <algorithm>

namespace moex_raw {

static void add_issue(std::vector<RawValidationIssue>& issues,
                      ValidationSeverity sev, const std::string& code,
                      const std::string& msg) {
    issues.push_back({sev, code, msg});
}

SegmentStatus validate_segment(const std::string& path,
                               RawSegmentMetadata& meta,
                               RawFooter& footer,
                               std::vector<RawValidationIssue>& issues,
                               std::string& content_sha256_hex,
                               std::string& file_sha256_hex) {
    // Open file for reading
    std::FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) {
        add_issue(issues, ValidationSeverity::Error, "IO_ERROR", "Cannot open file");
        return SegmentStatus::IoError;
    }

    // Get file size with error checking
    if (std::fseek(f, 0, SEEK_END) != 0) {
        std::fclose(f);
        add_issue(issues, ValidationSeverity::Error, "IO_ERROR", "Cannot seek to end");
        return SegmentStatus::IoError;
    }
    long file_size_long = std::ftell(f);
    if (file_size_long < 0) {
        std::fclose(f);
        add_issue(issues, ValidationSeverity::Error, "IO_ERROR", "Negative ftell");
        return SegmentStatus::IoError;
    }
    auto file_size = static_cast<std::uint64_t>(file_size_long);
    if (std::fseek(f, 0, SEEK_SET) != 0) {
        std::fclose(f);
        add_issue(issues, ValidationSeverity::Error, "IO_ERROR", "Cannot seek to start");
        return SegmentStatus::IoError;
    }

    if (file_size == 0) {
        std::fclose(f);
        add_issue(issues, ValidationSeverity::Error, "EMPTY_FILE", "Empty file");
        return SegmentStatus::Truncated;
    }

    // Check minimum size: header min + footer
    if (file_size < 126 + kFooterSize) {
        std::fclose(f);
        add_issue(issues, ValidationSeverity::Error, "FILE_TOO_SHORT",
                  "File too short for header + footer");
        return SegmentStatus::Truncated;
    }

    // --- Read header (bounded: up to kMaxHeaderSize) ---
    std::size_t header_read_size = static_cast<std::size_t>(std::min(file_size, static_cast<std::uint64_t>(kMaxHeaderSize)));
    std::vector<std::uint8_t> header_buf(header_read_size);
    if (std::fread(header_buf.data(), 1, header_read_size, f) != header_read_size) {
        std::fclose(f);
        add_issue(issues, ValidationSeverity::Error, "IO_ERROR", "Cannot read header");
        return SegmentStatus::IoError;
    }

    std::size_t header_size = 0;
    if (!deserialize_header(header_buf.data(), header_read_size, meta, header_size, issues)) {
        std::fclose(f);
        // Check if it's partial
        if (file_size >= 8) {
            // Read last 8 bytes to check for footer magic
            std::FILE* f2 = std::fopen(path.c_str(), "rb");
            if (f2) {
                std::uint8_t last8[8];
                std::fseek(f2, -8, SEEK_END);
                if (std::fread(last8, 1, 8, f2) == 8 && std::memcmp(last8, kMagicEnd, 8) != 0) {
                    std::fclose(f2);
                    return SegmentStatus::Partial;
                }
                std::fclose(f2);
            }
        }
        return SegmentStatus::Corrupt;
    }

    // --- Read footer (last kFooterSize bytes) ---
    if (file_size < header_size + kFooterSize) {
        std::fclose(f);
        add_issue(issues, ValidationSeverity::Error, "TRUNCATED",
                  "File too short for footer after header");
        return SegmentStatus::Truncated;
    }

    std::uint64_t footer_offset = file_size - kFooterSize;
    std::uint8_t footer_buf_arr[kFooterSize];
    if (std::fseek(f, static_cast<long>(footer_offset), SEEK_SET) != 0) {
        std::fclose(f);
        add_issue(issues, ValidationSeverity::Error, "IO_ERROR", "Cannot seek to footer");
        return SegmentStatus::IoError;
    }
    if (std::fread(footer_buf_arr, 1, kFooterSize, f) != kFooterSize) {
        std::fclose(f);
        add_issue(issues, ValidationSeverity::Error, "IO_ERROR", "Cannot read footer");
        return SegmentStatus::IoError;
    }

    if (!deserialize_footer(footer_buf_arr, kFooterSize, footer, issues)) {
        std::fclose(f);
        return SegmentStatus::Corrupt;
    }

    // Verify data_bytes_before_footer matches
    if (footer.data_bytes_before_footer != footer_offset) {
        std::fclose(f);
        add_issue(issues, ValidationSeverity::Error, "WRONG_DATA_BYTES",
                  "data_bytes_before_footer mismatch");
        return SegmentStatus::Corrupt;
    }

    // --- Compute content SHA-256 incrementally ---
    // Seek back to start and hash everything before footer
    if (std::fseek(f, 0, SEEK_SET) != 0) {
        std::fclose(f);
        add_issue(issues, ValidationSeverity::Error, "IO_ERROR", "Cannot seek for SHA-256");
        return SegmentStatus::IoError;
    }

    SHA256Ctx sha_ctx;
    sha256_init(sha_ctx);
    std::uint8_t read_buf[4096];
    std::uint64_t remaining = footer_offset;
    while (remaining > 0) {
        std::size_t to_read = static_cast<std::size_t>(std::min(remaining, static_cast<std::uint64_t>(sizeof(read_buf))));
        std::size_t n = std::fread(read_buf, 1, to_read, f);
        if (n == 0) {
            std::fclose(f);
            add_issue(issues, ValidationSeverity::Error, "IO_ERROR", "Read error during SHA-256");
            return SegmentStatus::IoError;
        }
        sha256_update(sha_ctx, read_buf, n);
        remaining -= n;
    }
    std::uint8_t computed_content_hash[32];
    sha256_final(sha_ctx, computed_content_hash);
    content_sha256_hex = sha256_bytes_to_hex(computed_content_hash);

    std::string stored_content_hex = sha256_bytes_to_hex(footer.content_sha256);
    if (content_sha256_hex != stored_content_hex) {
        std::fclose(f);
        add_issue(issues, ValidationSeverity::Error, "WRONG_CONTENT_SHA256",
                  "content_sha256 mismatch");
        return SegmentStatus::Corrupt;
    }

    // --- Compute file SHA-256 (entire file) ---
    if (std::fseek(f, 0, SEEK_SET) != 0) {
        std::fclose(f);
        add_issue(issues, ValidationSeverity::Error, "IO_ERROR", "Cannot seek for file SHA-256");
        return SegmentStatus::IoError;
    }
    SHA256Ctx file_sha_ctx;
    sha256_init(file_sha_ctx);
    std::uint64_t file_remaining = file_size;
    while (file_remaining > 0) {
        std::size_t to_read = static_cast<std::size_t>(std::min(file_remaining, static_cast<std::uint64_t>(sizeof(read_buf))));
        std::size_t n = std::fread(read_buf, 1, to_read, f);
        if (n == 0) break;
        sha256_update(file_sha_ctx, read_buf, n);
        file_remaining -= n;
    }
    std::uint8_t file_hash[32];
    sha256_final(file_sha_ctx, file_hash);
    file_sha256_hex = sha256_bytes_to_hex(file_hash);

    // --- Validate records streaming (bounded: one record at a time) ---
    if (std::fseek(f, static_cast<long>(header_size), SEEK_SET) != 0) {
        std::fclose(f);
        add_issue(issues, ValidationSeverity::Error, "IO_ERROR", "Cannot seek to records");
        return SegmentStatus::IoError;
    }

    std::uint64_t expected_capture_index = meta.start_capture_index;
    std::uint64_t last_monotonic_ns = 0;
    std::uint64_t actual_record_count = 0;
    std::uint64_t actual_payload_bytes = 0;
    std::uint64_t actual_first_capture = 0;
    std::uint64_t actual_last_capture = 0;
    bool first_record = true;
    std::uint64_t pos = header_size;

    while (pos < footer_offset) {
        // Read record header (bounded: kRecordHeaderSize bytes)
        std::uint8_t rec_hdr_buf[kRecordHeaderSize];
        if (std::fread(rec_hdr_buf, 1, kRecordHeaderSize, f) != kRecordHeaderSize) {
            std::fclose(f);
            add_issue(issues, ValidationSeverity::Error, "IO_ERROR", "Cannot read record header");
            return SegmentStatus::Corrupt;
        }

        // Parse record size from header to know how much more to read
        if (std::memcmp(rec_hdr_buf, kMagicRec, 4) != 0) {
            std::fclose(f);
            add_issue(issues, ValidationSeverity::Error, "WRONG_RECORD_MAGIC", "Wrong record magic");
            return SegmentStatus::Corrupt;
        }
        std::uint32_t record_size = read_u32_le(rec_hdr_buf + 8);
        std::uint32_t payload_size = read_u32_le(rec_hdr_buf + 36);

        // Validate record_size
        if (record_size != kRecordHeaderSize + payload_size + 4) {
            std::fclose(f);
            add_issue(issues, ValidationSeverity::Error, "WRONG_RECORD_SIZE", "record_size mismatch");
            return SegmentStatus::Corrupt;
        }
        if (payload_size > kMaxPayloadSize) {
            std::fclose(f);
            add_issue(issues, ValidationSeverity::Error, "PAYLOAD_TOO_LARGE", "Payload exceeds 1 MiB");
            return SegmentStatus::Corrupt;
        }

        // Check we have enough data
        if (pos + record_size > footer_offset) {
            std::fclose(f);
            add_issue(issues, ValidationSeverity::Error, "TRUNCATED_RECORD", "Truncated record");
            return SegmentStatus::Corrupt;
        }

        // Read remaining record bytes (payload + payload_crc + record_crc)
        std::size_t remaining_bytes = record_size - kRecordHeaderSize;
        std::vector<std::uint8_t> rec_tail(remaining_bytes);
        if (std::fread(rec_tail.data(), 1, remaining_bytes, f) != remaining_bytes) {
            std::fclose(f);
            add_issue(issues, ValidationSeverity::Error, "IO_ERROR", "Cannot read record tail");
            return SegmentStatus::Corrupt;
        }

        // Build full record for deserialization
        std::vector<std::uint8_t> full_rec(rec_hdr_buf, rec_hdr_buf + kRecordHeaderSize);
        full_rec.insert(full_rec.end(), rec_tail.begin(), rec_tail.end());

        RawPacketRecord rec;
        std::size_t record_total_size = 0;
        if (!deserialize_record_header(full_rec.data(), full_rec.size(), rec, record_total_size, issues)) {
            std::fclose(f);
            return SegmentStatus::Corrupt;
        }

        // Validate capture_index continuity
        if (rec.capture_index != expected_capture_index) {
            std::fclose(f);
            add_issue(issues, ValidationSeverity::Error, "WRONG_CAPTURE_INDEX",
                      "capture_index not contiguous");
            return SegmentStatus::Corrupt;
        }

        // Validate monotonic timestamp
        if (rec.capture_monotonic_ns < last_monotonic_ns) {
            std::fclose(f);
            add_issue(issues, ValidationSeverity::Error, "MONOTONIC_DECREASE",
                      "capture_monotonic_ns decreased");
            return SegmentStatus::Corrupt;
        }
        last_monotonic_ns = rec.capture_monotonic_ns;

        if (first_record) {
            actual_first_capture = rec.capture_index;
            first_record = false;
        }
        actual_last_capture = rec.capture_index;
        actual_record_count++;
        actual_payload_bytes += rec.payload.size();

        expected_capture_index++;
        pos += record_size;
    }

    std::fclose(f);

    // Validate footer counts
    if (actual_record_count != footer.record_count) {
        add_issue(issues, ValidationSeverity::Error, "WRONG_RECORD_COUNT",
                  "Record count mismatch");
        return SegmentStatus::Corrupt;
    }
    if (actual_payload_bytes != footer.total_payload_bytes) {
        add_issue(issues, ValidationSeverity::Error, "WRONG_PAYLOAD_TOTAL",
                  "Total payload bytes mismatch");
        return SegmentStatus::Corrupt;
    }
    if (actual_first_capture != footer.first_capture_index) {
        add_issue(issues, ValidationSeverity::Error, "WRONG_FIRST_INDEX",
                  "first_capture_index mismatch");
        return SegmentStatus::Corrupt;
    }
    if (actual_last_capture != footer.last_capture_index) {
        add_issue(issues, ValidationSeverity::Error, "WRONG_LAST_INDEX",
                  "last_capture_index mismatch");
        return SegmentStatus::Corrupt;
    }

    return SegmentStatus::ValidFinalized;
}

SegmentStatus validate_stream_set(const std::vector<std::string>& paths,
                                  std::vector<RawSegmentMetadata>& metas,
                                  std::vector<RawFooter>& footers,
                                  std::vector<RawValidationIssue>& issues) {
    if (paths.empty()) {
        add_issue(issues, ValidationSeverity::Error, "EMPTY_STREAM_SET", "No segments provided");
        return SegmentStatus::Corrupt;
    }

    metas.clear();
    footers.clear();
    metas.reserve(paths.size());
    footers.reserve(paths.size());

    // Validate each segment and collect metadata
    for (const auto& path : paths) {
        RawSegmentMetadata meta;
        RawFooter footer;
        std::vector<RawValidationIssue> seg_issues;
        std::string content_hex, file_hex;

        auto status = validate_segment(path, meta, footer, seg_issues, content_hex, file_hex);
        issues.insert(issues.end(), seg_issues.begin(), seg_issues.end());

        if (status != SegmentStatus::ValidFinalized) {
            add_issue(issues, ValidationSeverity::Error, "INVALID_SEGMENT",
                      "Segment not valid finalized: " + path);
            return status;
        }

        metas.push_back(meta);
        footers.push_back(footer);
    }

    // Check consistent metadata
    for (std::size_t i = 1; i < metas.size(); ++i) {
        if (std::memcmp(metas[i].session.session_id, metas[0].session.session_id, 16) != 0) {
            add_issue(issues, ValidationSeverity::Error, "MIXED_SESSION_ID",
                      "Different session_id in stream set");
            return SegmentStatus::Corrupt;
        }
        if (metas[i].source.source_id != metas[0].source.source_id) {
            add_issue(issues, ValidationSeverity::Error, "MIXED_SOURCE_ID",
                      "Different source_id in stream set");
            return SegmentStatus::Corrupt;
        }
        if (metas[i].source.channel_id != metas[0].source.channel_id) {
            add_issue(issues, ValidationSeverity::Error, "MIXED_CHANNEL_ID",
                      "Different channel_id in stream set");
            return SegmentStatus::Corrupt;
        }
    }

    // Check contiguous segment indexes
    for (std::size_t i = 1; i < metas.size(); ++i) {
        if (metas[i].segment_index != metas[i-1].segment_index + 1) {
            add_issue(issues, ValidationSeverity::Error, "NONCONTIGUOUS_SEGMENT_INDEX",
                      "Segment indexes not contiguous");
            return SegmentStatus::Corrupt;
        }
    }

    // Check contiguous capture indexes across segments
    for (std::size_t i = 1; i < footers.size(); ++i) {
        if (footers[i].first_capture_index != footers[i-1].last_capture_index + 1) {
            add_issue(issues, ValidationSeverity::Error, "NONCONTIGUOUS_CAPTURE_INDEX",
                      "capture_index not contiguous across segments");
            return SegmentStatus::Corrupt;
        }
    }

    return SegmentStatus::ValidFinalized;
}

std::vector<StreamSetInfo> group_stream_sets(const std::string& directory) {
    std::vector<StreamSetInfo> result;
    std::vector<std::string> unparsed_files;

    if (!std::filesystem::exists(directory)) return result;

    for (const auto& entry : std::filesystem::directory_iterator(directory)) {
        if (!entry.is_regular_file()) continue;

        auto filename = entry.path().filename().string();
        auto ext = entry.path().extension().string();

        // Only process .mxraw files (not .partial)
        if (ext != ".mxraw") continue;
        if (filename.find(".partial") != std::string::npos) continue;

        // Read header to get session/source/channel (bounded: 4096 bytes)
        std::FILE* f = std::fopen(entry.path().string().c_str(), "rb");
        if (!f) {
            unparsed_files.push_back(entry.path().string());
            continue;
        }

        std::vector<std::uint8_t> header_data(kMaxHeaderSize);
        std::size_t n = std::fread(header_data.data(), 1, kMaxHeaderSize, f);
        std::fclose(f);

        RawSegmentMetadata meta;
        std::size_t header_size = 0;
        std::vector<RawValidationIssue> issues;

        if (!deserialize_header(header_data.data(), n, meta, header_size, issues)) {
            unparsed_files.push_back(entry.path().string());
            continue;
        }

        // Find or create stream set using full key (session_id, source_id, channel_id)
        StreamSetInfo* found = nullptr;
        for (auto& ss : result) {
            if (std::memcmp(ss.session_id, meta.session.session_id, 16) == 0 &&
                ss.source_id == meta.source.source_id &&
                ss.channel_id == meta.source.channel_id) {
                found = &ss;
                break;
            }
        }

        if (!found) {
            result.push_back({});
            found = &result.back();
            std::memcpy(found->session_id, meta.session.session_id, 16);
            found->source_id = meta.source.source_id;
            found->channel_id = meta.source.channel_id;
        }

        found->segment_paths.push_back(entry.path().string());
        found->segment_indexes.push_back(meta.segment_index);
    }

    // Sort each stream set by segment index
    for (auto& ss : result) {
        std::vector<std::pair<std::uint64_t, std::string>> pairs;
        for (std::size_t i = 0; i < ss.segment_indexes.size(); ++i) {
            pairs.emplace_back(ss.segment_indexes[i], ss.segment_paths[i]);
        }
        std::sort(pairs.begin(), pairs.end());
        for (std::size_t i = 0; i < pairs.size(); ++i) {
            ss.segment_indexes[i] = pairs[i].first;
            ss.segment_paths[i] = pairs[i].second;
        }
    }

    return result;
}

}  // namespace moex_raw
