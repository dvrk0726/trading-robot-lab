#include "moex_raw/raw_segment.hpp"
#include "moex_raw/raw_types.hpp"
#include "moex_raw/endian.hpp"
#include "moex_raw/crc32c.hpp"
#include "moex_raw/sha256.hpp"
#include "moex_raw/file_position.hpp"
#include <cstdio>
#include <filesystem>
#include <cstring>
#include <algorithm>

namespace moex_raw {

static void add_issue(std::vector<RawValidationIssue>& issues,
                      ValidationSeverity sev, const std::string& code,
                      const std::string& msg) {
    issues.push_back({sev, code, msg, {}, {}});
}

SegmentStatus validate_segment(const std::string& path,
                               RawSegmentMetadata& meta,
                               RawFooter& footer,
                               std::vector<RawValidationIssue>& issues,
                               std::string& content_sha256_hex,
                               std::string& file_sha256_hex,
                               std::uint64_t* first_monotonic_ns,
                               std::uint64_t* last_monotonic_ns) {
    // Open file for reading
    std::FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) {
        add_issue(issues, ValidationSeverity::Error, "IO_ERROR", "Cannot open file");
        return SegmentStatus::IoError;
    }

    // Get file size using std::filesystem (supports > 4 GiB on all platforms)
    std::error_code ec;
    auto file_size = static_cast<std::uint64_t>(std::filesystem::file_size(path, ec));
    if (ec) {
        std::fclose(f);
        add_issue(issues, ValidationSeverity::Error, "IO_ERROR", "Cannot get file size");
        return SegmentStatus::IoError;
    }

    // Reject files above hard 64 GiB cap
    if (file_size > kMaxSegmentBytes) {
        std::fclose(f);
        add_issue(issues, ValidationSeverity::Error, "FILE_TOO_LARGE",
                  "File exceeds 64 GiB hard limit");
        return SegmentStatus::Unsupported;
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
        // Check if this is an unsupported version
        if (header_read_size >= 10) {
            std::uint16_t version = read_u16_le(header_buf.data() + 8);
            if (version != kFormatVersion) {
                return SegmentStatus::Unsupported;
            }
        }
        // Header failure without version mismatch: check if file has footer magic at correct position
        // Footer magic is at the START of the 92-byte footer, not at EOF-8
        if (file_size >= kFooterSize) {
            std::FILE* f2 = std::fopen(path.c_str(), "rb");
            if (f2) {
                std::uint64_t footer_offset = file_size - kFooterSize;
                std::uint8_t footer_magic[8];
                if (fseek64(f2, static_cast<std::int64_t>(footer_offset), SEEK_SET) == 0 &&
                    std::fread(footer_magic, 1, 8, f2) == 8) {
                    if (std::memcmp(footer_magic, kMagicEnd, 8) != 0) {
                        // No valid footer magic => partial/unfinalized
                        std::fclose(f2);
                        return SegmentStatus::Partial;
                    }
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
    if (fseek64(f, static_cast<std::int64_t>(footer_offset), SEEK_SET) != 0) {
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
        if (n == 0) {
            std::fclose(f);
            add_issue(issues, ValidationSeverity::Error, "IO_ERROR",
                      "Short read during file SHA-256");
            return SegmentStatus::IoError;
        }
        sha256_update(file_sha_ctx, read_buf, n);
        file_remaining -= n;
    }
    std::uint8_t file_hash[32];
    sha256_final(file_sha_ctx, file_hash);
    file_sha256_hex = sha256_bytes_to_hex(file_hash);

    // --- Validate records streaming (bounded: one record at a time) ---
    if (fseek64(f, static_cast<std::int64_t>(header_size), SEEK_SET) != 0) {
        std::fclose(f);
        add_issue(issues, ValidationSeverity::Error, "IO_ERROR", "Cannot seek to records");
        return SegmentStatus::IoError;
    }

    std::uint64_t expected_capture_index = meta.start_capture_index;
    std::uint64_t local_last_monotonic = 0;
    std::uint64_t local_first_monotonic = 0;
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
        if (rec.capture_monotonic_ns < local_last_monotonic) {
            std::fclose(f);
            add_issue(issues, ValidationSeverity::Error, "MONOTONIC_DECREASE",
                      "capture_monotonic_ns decreased");
            return SegmentStatus::Corrupt;
        }
        local_last_monotonic = rec.capture_monotonic_ns;

        if (first_record) {
            actual_first_capture = rec.capture_index;
            local_first_monotonic = rec.capture_monotonic_ns;
            first_record = false;
        }
        actual_last_capture = rec.capture_index;
        actual_record_count++;
        actual_payload_bytes += rec.payload.size();

        // Use checked arithmetic for index advancement
        std::uint64_t next_index;
        if (!checked_add_u64(expected_capture_index, 1, next_index)) {
            std::fclose(f);
            add_issue(issues, ValidationSeverity::Error, "INDEX_OVERFLOW",
                      "capture_index overflow");
            return SegmentStatus::Corrupt;
        }
        expected_capture_index = next_index;
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

    // Output first/last monotonic timestamps
    if (first_monotonic_ns) *first_monotonic_ns = local_first_monotonic;
    if (last_monotonic_ns) *last_monotonic_ns = local_last_monotonic;

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

    // Phase 1: Parse canonical filenames and validate each segment
    struct SegmentEntry {
        std::string path;
        ParsedFilename parsed_fn;
        RawSegmentMetadata meta;
        RawFooter footer;
        std::uint64_t file_size;
        std::string content_sha256_hex;
        std::string file_sha256_hex;
        std::uint64_t first_monotonic;
        std::uint64_t last_monotonic;
    };

    std::vector<SegmentEntry> entries;
    entries.reserve(paths.size());

    for (const auto& path : paths) {
        SegmentEntry entry;
        entry.path = path;
        entry.first_monotonic = 0;
        entry.last_monotonic = 0;

        // Parse canonical filename
        auto filename = std::filesystem::path(path).filename().string();
        if (!parse_canonical_filename(filename, entry.parsed_fn)) {
            add_issue(issues, ValidationSeverity::Error, "INVALID_FILENAME",
                      "Cannot parse canonical filename: " + filename);
            return SegmentStatus::Corrupt;
        }

        // Get file size
        std::error_code ec;
        entry.file_size = static_cast<std::uint64_t>(std::filesystem::file_size(path, ec));
        if (ec) {
            add_issue(issues, ValidationSeverity::Error, "IO_ERROR",
                      "Cannot get file size: " + path);
            return SegmentStatus::IoError;
        }

        // Validate the segment — collect first/last monotonic during the main pass
        std::vector<RawValidationIssue> seg_issues;
        auto status = validate_segment(path, entry.meta, entry.footer, seg_issues,
                                       entry.content_sha256_hex, entry.file_sha256_hex,
                                       &entry.first_monotonic, &entry.last_monotonic);
        issues.insert(issues.end(), seg_issues.begin(), seg_issues.end());

        if (status != SegmentStatus::ValidFinalized) {
            add_issue(issues, ValidationSeverity::Error, "INVALID_SEGMENT",
                      "Segment not valid finalized: " + path);
            return status;
        }

        // Compare filename identity with content identity
        if (std::memcmp(entry.parsed_fn.session_id, entry.meta.session.session_id, 16) != 0) {
            add_issue(issues, ValidationSeverity::Error, "FILENAME_MISMATCH",
                      "Filename session_id does not match content: " + filename);
            return SegmentStatus::Corrupt;
        }
        if (entry.parsed_fn.source_id != entry.meta.source.source_id) {
            add_issue(issues, ValidationSeverity::Error, "FILENAME_MISMATCH",
                      "Filename source_id does not match content: " + filename);
            return SegmentStatus::Corrupt;
        }
        if (entry.parsed_fn.channel_id != entry.meta.source.channel_id) {
            add_issue(issues, ValidationSeverity::Error, "FILENAME_MISMATCH",
                      "Filename channel_id does not match content: " + filename);
            return SegmentStatus::Corrupt;
        }
        if (entry.parsed_fn.segment_index != entry.meta.segment_index) {
            add_issue(issues, ValidationSeverity::Error, "FILENAME_MISMATCH",
                      "Filename segment_index does not match content: " + filename);
            return SegmentStatus::Corrupt;
        }

        entries.push_back(std::move(entry));
    }

    // Phase 2: Sort numerically by segment index (independent of input order)
    std::sort(entries.begin(), entries.end(),
              [](const SegmentEntry& a, const SegmentEntry& b) {
                  return a.meta.segment_index < b.meta.segment_index;
              });

    // Phase 3: Check for duplicate segment indexes
    for (std::size_t i = 1; i < entries.size(); ++i) {
        if (entries[i].meta.segment_index == entries[i - 1].meta.segment_index) {
            add_issue(issues, ValidationSeverity::Error, "DUPLICATE_SEGMENT_INDEX",
                      "Duplicate segment index: " + std::to_string(entries[i].meta.segment_index));
            return SegmentStatus::Corrupt;
        }
    }

    // Phase 4: Check contiguous segment indexes
    for (std::size_t i = 1; i < entries.size(); ++i) {
        if (entries[i].meta.segment_index != entries[i - 1].meta.segment_index + 1) {
            add_issue(issues, ValidationSeverity::Error, "NONCONTIGUOUS_SEGMENT_INDEX",
                      "Segment indexes not contiguous: " +
                      std::to_string(entries[i - 1].meta.segment_index) + " -> " +
                      std::to_string(entries[i].meta.segment_index));
            return SegmentStatus::Corrupt;
        }
    }

    // Phase 5: Check consistent metadata across all segments
    for (std::size_t i = 1; i < entries.size(); ++i) {
        const auto& a = entries[0].meta;
        const auto& b = entries[i].meta;

        if (std::memcmp(b.session.session_id, a.session.session_id, 16) != 0) {
            add_issue(issues, ValidationSeverity::Error, "MIXED_SESSION_ID",
                      "Different session_id in stream set");
            return SegmentStatus::Corrupt;
        }
        if (b.source.source_id != a.source.source_id) {
            add_issue(issues, ValidationSeverity::Error, "MIXED_SOURCE_ID",
                      "Different source_id in stream set");
            return SegmentStatus::Corrupt;
        }
        if (b.source.channel_id != a.source.channel_id) {
            add_issue(issues, ValidationSeverity::Error, "MIXED_CHANNEL_ID",
                      "Different channel_id in stream set");
            return SegmentStatus::Corrupt;
        }
        if (b.session.feed_group != a.session.feed_group) {
            add_issue(issues, ValidationSeverity::Error, "MIXED_FEED_GROUP",
                      "Different feed_group in stream set");
            return SegmentStatus::Corrupt;
        }
        if (b.session.endpoint_role != a.session.endpoint_role) {
            add_issue(issues, ValidationSeverity::Error, "MIXED_ENDPOINT_ROLE",
                      "Different endpoint_role in stream set");
            return SegmentStatus::Corrupt;
        }
        if (b.session.source_label != a.session.source_label) {
            add_issue(issues, ValidationSeverity::Error, "MIXED_SOURCE_LABEL",
                      "Different source_label in stream set");
            return SegmentStatus::Corrupt;
        }
        if (b.source.clock_domain != a.source.clock_domain) {
            add_issue(issues, ValidationSeverity::Error, "MIXED_CLOCK_DOMAIN",
                      "Different clock_domain in stream set");
            return SegmentStatus::Corrupt;
        }
        if (b.source.transport != a.source.transport) {
            add_issue(issues, ValidationSeverity::Error, "MIXED_TRANSPORT",
                      "Different transport in stream set");
            return SegmentStatus::Corrupt;
        }
        if (b.source.source_side != a.source.source_side) {
            add_issue(issues, ValidationSeverity::Error, "MIXED_SOURCE_SIDE",
                      "Different source_side in stream set");
            return SegmentStatus::Corrupt;
        }
        if (std::memcmp(b.source.configuration_sha256, a.source.configuration_sha256, 32) != 0) {
            add_issue(issues, ValidationSeverity::Error, "MIXED_CONFIGURATION_HASH",
                      "Different configuration_sha256 in stream set");
            return SegmentStatus::Corrupt;
        }
        if (std::memcmp(b.source.templates_sha256, a.source.templates_sha256, 32) != 0) {
            add_issue(issues, ValidationSeverity::Error, "MIXED_TEMPLATES_HASH",
                      "Different templates_sha256 in stream set");
            return SegmentStatus::Corrupt;
        }
        if (std::memcmp(b.source.endpoint_fingerprint_sha256, a.source.endpoint_fingerprint_sha256, 32) != 0) {
            add_issue(issues, ValidationSeverity::Error, "MIXED_FINGERPRINT_HASH",
                      "Different endpoint_fingerprint_sha256 in stream set");
            return SegmentStatus::Corrupt;
        }
    }

    // Phase 6: Check contiguous capture indexes and cross-segment monotonic timestamps
    for (std::size_t i = 1; i < entries.size(); ++i) {
        if (entries[i].footer.first_capture_index != entries[i - 1].footer.last_capture_index + 1) {
            add_issue(issues, ValidationSeverity::Error, "NONCONTIGUOUS_CAPTURE_INDEX",
                      "capture_index not contiguous across segments");
            return SegmentStatus::Corrupt;
        }
        // Cross-segment monotonic: first(current) must be >= last(previous)
        if (entries[i].first_monotonic < entries[i - 1].last_monotonic) {
            add_issue(issues, ValidationSeverity::Error, "MONOTONIC_DECREASE_CROSS_SEGMENT",
                      "capture_monotonic_ns decreased across segment boundary");
            return SegmentStatus::Corrupt;
        }
    }

    // Phase 7: Populate output vectors
    metas.clear();
    footers.clear();
    metas.reserve(entries.size());
    footers.reserve(entries.size());
    for (const auto& e : entries) {
        metas.push_back(e.meta);
        footers.push_back(e.footer);
    }

    return SegmentStatus::ValidFinalized;
}

std::vector<StreamSetInfo> group_stream_sets(const std::string& directory,
                                              std::vector<RawValidationIssue>& issues) {
    std::vector<StreamSetInfo> result;

    if (!std::filesystem::exists(directory)) return result;

    for (const auto& entry : std::filesystem::directory_iterator(directory)) {
        if (!entry.is_regular_file()) continue;

        auto filepath = entry.path().string();
        auto filename = entry.path().filename().string();

        // Handle .mxraw.partial files as partial candidates
        if (filename.size() > 14 && filename.substr(filename.size() - 14) == ".mxraw.partial") {
            add_issue(issues, ValidationSeverity::Warning, "PARTIAL_FILE",
                      "Partial segment file found: " + filepath);
            continue;
        }

        // Only process .mxraw files
        auto ext = entry.path().extension().string();
        if (ext != ".mxraw") continue;

        // Parse canonical filename first
        ParsedFilename parsed_fn;
        if (!parse_canonical_filename(filename, parsed_fn)) {
            add_issue(issues, ValidationSeverity::Error, "INVALID_FILENAME",
                      "Cannot parse canonical filename: " + filepath);
            continue;
        }

        // Read header to get session/source/channel (bounded: 4096 bytes)
        std::FILE* f = std::fopen(filepath.c_str(), "rb");
        if (!f) {
            add_issue(issues, ValidationSeverity::Error, "UNREADABLE_FILE",
                      "Cannot open .mxraw file: " + filepath);
            continue;
        }

        std::vector<std::uint8_t> header_data(kMaxHeaderSize);
        std::size_t n = std::fread(header_data.data(), 1, kMaxHeaderSize, f);
        std::fclose(f);

        if (n == 0) {
            add_issue(issues, ValidationSeverity::Error, "EMPTY_FILE",
                      "Empty .mxraw file: " + filepath);
            continue;
        }

        RawSegmentMetadata meta;
        std::size_t header_size = 0;
        std::vector<RawValidationIssue> hdr_issues;

        if (!deserialize_header(header_data.data(), n, meta, header_size, hdr_issues)) {
            add_issue(issues, ValidationSeverity::Error, "MALFORMED_HEADER",
                      "Cannot parse .mxraw header: " + filepath);
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

        found->segment_paths.push_back(filepath);
        found->segment_indexes.push_back(meta.segment_index);
    }

    // Sort each stream set by parsed segment index (numeric, not lexical)
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
