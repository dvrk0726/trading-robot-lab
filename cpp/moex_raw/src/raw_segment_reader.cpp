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

static DefaultFileSystem g_default_fs;

static void add_issue(std::vector<RawValidationIssue>& issues,
                      ValidationSeverity sev, const std::string& code,
                      const std::string& msg, const std::string& path = {}) {
    issues.push_back({sev, code, msg, {}, path});
}

SegmentStatus validate_segment(const std::string& path,
                               RawSegmentMetadata& meta,
                               RawFooter& footer,
                               std::vector<RawValidationIssue>& issues,
                               std::string& content_sha256_hex,
                               std::string& file_sha256_hex,
                               std::uint64_t* first_monotonic_ns,
                               std::uint64_t* last_monotonic_ns,
                               std::uint64_t* first_utc_ns,
                               std::uint64_t* last_utc_ns,
                               IFileSystem* fs_arg) {
    auto* fs = fs_arg ? fs_arg : &g_default_fs;

    // Get file size using IFileSystem (reject > 64 GiB BEFORE opening)
    bool size_ok = false;
    auto file_size = fs->file_size(path, size_ok);
    if (!size_ok) {
        add_issue(issues, ValidationSeverity::Error, "IO_ERROR", "Cannot get file size", path);
        return SegmentStatus::IoError;
    }

    // Reject files above hard 64 GiB cap — before allocation/read
    if (file_size > kMaxSegmentBytes) {
        add_issue(issues, ValidationSeverity::Error, "FILE_TOO_LARGE",
                  "File exceeds 64 GiB hard limit", path);
        return SegmentStatus::Unsupported;
    }

    if (file_size == 0) {
        add_issue(issues, ValidationSeverity::Error, "EMPTY_FILE", "Empty file", path);
        return SegmentStatus::Truncated;
    }

    // Check minimum size: header min + footer
    // For .mxraw.partial files, being too short is expected (Partial, not Truncated)
    bool is_partial_ext = (path.size() > 14 &&
                           path.substr(path.size() - 14) == ".mxraw.partial");
    if (file_size < 126 + kFooterSize) {
        if (is_partial_ext) {
            add_issue(issues, ValidationSeverity::Warning, "PARTIAL_FILE",
                      "Partial segment file incomplete", path);
            return SegmentStatus::Partial;
        }
        add_issue(issues, ValidationSeverity::Error, "FILE_TOO_SHORT",
                  "File too short for header + footer", path);
        return SegmentStatus::Truncated;
    }

    // Open file for reading via IFileSystem
    auto f = fs->open_read(path);
    if (!f) {
        add_issue(issues, ValidationSeverity::Error, "IO_ERROR", "Cannot open file", path);
        return SegmentStatus::IoError;
    }

    // --- Read header (bounded: up to kMaxHeaderSize) ---
    std::size_t header_read_size = static_cast<std::size_t>(std::min(file_size, static_cast<std::uint64_t>(kMaxHeaderSize)));
    std::vector<std::uint8_t> header_buf(header_read_size);
    if (f->read(header_buf.data(), header_read_size) != header_read_size) {
        add_issue(issues, ValidationSeverity::Error, "IO_ERROR", "Cannot read header", path);
        return SegmentStatus::IoError;
    }

    std::size_t header_size = 0;
    std::size_t issues_before = issues.size();
    if (!deserialize_header(header_buf.data(), header_read_size, meta, header_size, issues)) {
        // Propagate concrete path to all new low-level issues
        for (std::size_t i = issues_before; i < issues.size(); ++i) {
            if (issues[i].path.empty()) issues[i].path = path;
        }
        // Close the file handle before returning
        f.reset();

        // Check if this is an unsupported version
        if (header_read_size >= 10) {
            std::uint16_t version = read_u16_le(header_buf.data() + 8);
            if (version != kFormatVersion) {
                return SegmentStatus::Unsupported;
            }
        }

        // Header failure without version mismatch.
        // Classification depends on file extension:
        //   .mxraw.partial -> Partial (unfinalized)
        //   .mxraw (finalized) -> Corrupt/Truncated (claims finalized but header broken)
        // is_partial_ext already computed above

        if (file_size >= kFooterSize) {
            auto f2 = fs->open_read(path);
            if (f2) {
                std::uint64_t footer_offset = file_size - kFooterSize;
                std::uint8_t footer_magic[8];
                if (f2->seek(static_cast<std::int64_t>(footer_offset), SEEK_SET) &&
                    f2->read(footer_magic, 8) == 8) {
                    if (std::memcmp(footer_magic, kMagicEnd, 8) != 0) {
                        // No valid footer magic
                        return is_partial_ext ? SegmentStatus::Partial : SegmentStatus::Corrupt;
                    }
                }
            }
        }
        // For .mxraw.partial without footer check or with valid footer but bad header
        if (is_partial_ext) return SegmentStatus::Partial;
        return SegmentStatus::Corrupt;
    }

    // --- Read footer (last kFooterSize bytes) ---
    if (file_size < header_size + kFooterSize) {
        if (is_partial_ext) {
            add_issue(issues, ValidationSeverity::Warning, "PARTIAL_FILE",
                      "Partial segment file incomplete", path);
            return SegmentStatus::Partial;
        }
        add_issue(issues, ValidationSeverity::Error, "TRUNCATED",
                  "File too short for footer after header", path);
        return SegmentStatus::Truncated;
    }

    std::uint64_t footer_offset = file_size - kFooterSize;
    std::uint8_t footer_buf_arr[kFooterSize];
    if (!f->seek(static_cast<std::int64_t>(footer_offset), SEEK_SET)) {
        add_issue(issues, ValidationSeverity::Error, "IO_ERROR", "Cannot seek to footer", path);
        return SegmentStatus::IoError;
    }
    if (f->read(footer_buf_arr, kFooterSize) != kFooterSize) {
        add_issue(issues, ValidationSeverity::Error, "IO_ERROR", "Cannot read footer", path);
        return SegmentStatus::IoError;
    }

    std::size_t footer_issues_before = issues.size();
    if (!deserialize_footer(footer_buf_arr, kFooterSize, footer, issues)) {
        // Propagate concrete path to all new low-level issues
        for (std::size_t i = footer_issues_before; i < issues.size(); ++i) {
            if (issues[i].path.empty()) issues[i].path = path;
        }
        return SegmentStatus::Corrupt;
    }

    // Verify data_bytes_before_footer matches
    if (footer.data_bytes_before_footer != footer_offset) {
        add_issue(issues, ValidationSeverity::Error, "WRONG_DATA_BYTES",
                  "data_bytes_before_footer mismatch", path);
        return SegmentStatus::Corrupt;
    }

    // --- Compute content SHA-256 incrementally ---
    if (!f->seek(0, SEEK_SET)) {
        add_issue(issues, ValidationSeverity::Error, "IO_ERROR", "Cannot seek for SHA-256", path);
        return SegmentStatus::IoError;
    }

    SHA256Ctx sha_ctx;
    sha256_init(sha_ctx);
    std::uint8_t read_buf[4096];
    std::uint64_t remaining = footer_offset;
    while (remaining > 0) {
        std::size_t to_read = static_cast<std::size_t>(std::min(remaining, static_cast<std::uint64_t>(sizeof(read_buf))));
        std::size_t n = f->read(read_buf, to_read);
        if (n == 0) {
            add_issue(issues, ValidationSeverity::Error, "IO_ERROR", "Read error during SHA-256", path);
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
        add_issue(issues, ValidationSeverity::Error, "WRONG_CONTENT_SHA256",
                  "content_sha256 mismatch", path);
        return SegmentStatus::Corrupt;
    }

    // --- Compute file SHA-256 (entire file) ---
    if (!f->seek(0, SEEK_SET)) {
        add_issue(issues, ValidationSeverity::Error, "IO_ERROR", "Cannot seek for file SHA-256", path);
        return SegmentStatus::IoError;
    }
    SHA256Ctx file_sha_ctx;
    sha256_init(file_sha_ctx);
    std::uint64_t file_remaining = file_size;
    while (file_remaining > 0) {
        std::size_t to_read = static_cast<std::size_t>(std::min(file_remaining, static_cast<std::uint64_t>(sizeof(read_buf))));
        std::size_t n = f->read(read_buf, to_read);
        if (n == 0) {
            add_issue(issues, ValidationSeverity::Error, "IO_ERROR",
                      "Short read during file SHA-256", path);
            return SegmentStatus::IoError;
        }
        sha256_update(file_sha_ctx, read_buf, n);
        file_remaining -= n;
    }
    std::uint8_t file_hash[32];
    sha256_final(file_sha_ctx, file_hash);
    file_sha256_hex = sha256_bytes_to_hex(file_hash);

    // --- Validate records streaming (bounded: one record at a time) ---
    if (!f->seek(static_cast<std::int64_t>(header_size), SEEK_SET)) {
        add_issue(issues, ValidationSeverity::Error, "IO_ERROR", "Cannot seek to records", path);
        return SegmentStatus::IoError;
    }

    std::uint64_t expected_capture_index = meta.start_capture_index;
    std::uint64_t local_last_monotonic = 0;
    std::uint64_t local_first_monotonic = 0;
    std::uint64_t local_first_utc = 0;
    std::uint64_t local_last_utc = 0;
    std::uint64_t actual_record_count = 0;
    std::uint64_t actual_payload_bytes = 0;
    std::uint64_t actual_first_capture = 0;
    std::uint64_t actual_last_capture = 0;
    bool first_record = true;
    bool first_utc = true;
    std::uint64_t pos = header_size;

    while (pos < footer_offset) {
        // Read record header (bounded: kRecordHeaderSize bytes)
        std::uint8_t rec_hdr_buf[kRecordHeaderSize];
        if (f->read(rec_hdr_buf, kRecordHeaderSize) != kRecordHeaderSize) {
            add_issue(issues, ValidationSeverity::Error, "IO_ERROR", "Cannot read record header", path);
            return SegmentStatus::IoError;
        }

        // Parse record size from header to know how much more to read
        if (std::memcmp(rec_hdr_buf, kMagicRec, 4) != 0) {
            add_issue(issues, ValidationSeverity::Error, "WRONG_RECORD_MAGIC", "Wrong record magic", path);
            return SegmentStatus::Corrupt;
        }
        std::uint32_t record_size = read_u32_le(rec_hdr_buf + 8);
        std::uint32_t payload_size = read_u32_le(rec_hdr_buf + 36);

        // Validate record_size
        if (record_size != kRecordHeaderSize + payload_size + 4) {
            add_issue(issues, ValidationSeverity::Error, "WRONG_RECORD_SIZE", "record_size mismatch", path);
            return SegmentStatus::Corrupt;
        }
        if (payload_size > kMaxPayloadSize) {
            add_issue(issues, ValidationSeverity::Error, "PAYLOAD_TOO_LARGE", "Payload exceeds 1 MiB", path);
            return SegmentStatus::Corrupt;
        }

        // Check we have enough data
        if (pos + record_size > footer_offset) {
            add_issue(issues, ValidationSeverity::Error, "TRUNCATED_RECORD", "Truncated record", path);
            return SegmentStatus::Corrupt;
        }

        // Read remaining record bytes (payload + payload_crc + record_crc)
        std::size_t remaining_bytes = record_size - kRecordHeaderSize;
        std::vector<std::uint8_t> rec_tail(remaining_bytes);
        if (f->read(rec_tail.data(), remaining_bytes) != remaining_bytes) {
            add_issue(issues, ValidationSeverity::Error, "IO_ERROR", "Cannot read record tail", path);
            return SegmentStatus::IoError;
        }

        // Build full record for deserialization
        std::vector<std::uint8_t> full_rec(rec_hdr_buf, rec_hdr_buf + kRecordHeaderSize);
        full_rec.insert(full_rec.end(), rec_tail.begin(), rec_tail.end());

        RawPacketRecord rec;
        std::size_t record_total_size = 0;
        std::size_t rec_issues_before = issues.size();
        if (!deserialize_record_header(full_rec.data(), full_rec.size(), rec, record_total_size, issues)) {
            // Propagate concrete path to all new low-level issues
            for (std::size_t i = rec_issues_before; i < issues.size(); ++i) {
                if (issues[i].path.empty()) issues[i].path = path;
            }
            return SegmentStatus::Corrupt;
        }

        // Validate capture_index continuity
        if (rec.capture_index != expected_capture_index) {
            add_issue(issues, ValidationSeverity::Error, "WRONG_CAPTURE_INDEX",
                      "capture_index not contiguous", path);
            return SegmentStatus::Corrupt;
        }

        // Validate monotonic timestamp
        if (rec.capture_monotonic_ns < local_last_monotonic) {
            add_issue(issues, ValidationSeverity::Error, "MONOTONIC_DECREASE",
                      "capture_monotonic_ns decreased", path);
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

        // Collect UTC bounds from records with kRecordFlagUtcValid
        if (rec.record_flags & kRecordFlagUtcValid) {
            if (first_utc) {
                local_first_utc = rec.capture_utc_ns;
                first_utc = false;
            }
            local_last_utc = rec.capture_utc_ns;
        }

        // Use checked arithmetic for index advancement
        std::uint64_t next_index;
        if (!checked_add_u64(expected_capture_index, 1, next_index)) {
            add_issue(issues, ValidationSeverity::Error, "INDEX_OVERFLOW",
                      "capture_index overflow", path);
            return SegmentStatus::Corrupt;
        }
        expected_capture_index = next_index;
        pos += record_size;
    }

    // Validate footer counts
    if (actual_record_count != footer.record_count) {
        add_issue(issues, ValidationSeverity::Error, "WRONG_RECORD_COUNT",
                  "Record count mismatch", path);
        return SegmentStatus::Corrupt;
    }
    if (actual_payload_bytes != footer.total_payload_bytes) {
        add_issue(issues, ValidationSeverity::Error, "WRONG_PAYLOAD_TOTAL",
                  "Total payload bytes mismatch", path);
        return SegmentStatus::Corrupt;
    }
    if (actual_first_capture != footer.first_capture_index) {
        add_issue(issues, ValidationSeverity::Error, "WRONG_FIRST_INDEX",
                  "first_capture_index mismatch", path);
        return SegmentStatus::Corrupt;
    }
    if (actual_last_capture != footer.last_capture_index) {
        add_issue(issues, ValidationSeverity::Error, "WRONG_LAST_INDEX",
                  "last_capture_index mismatch", path);
        return SegmentStatus::Corrupt;
    }

    // Output first/last monotonic timestamps
    if (first_monotonic_ns) *first_monotonic_ns = local_first_monotonic;
    if (last_monotonic_ns) *last_monotonic_ns = local_last_monotonic;

    // Output first/last UTC (0 if no records had kRecordFlagUtcValid)
    if (first_utc_ns) *first_utc_ns = local_first_utc;
    if (last_utc_ns) *last_utc_ns = local_last_utc;

    // .mxraw.partial files are always Partial, even if they happen to have valid content
    if (is_partial_ext) {
        add_issue(issues, ValidationSeverity::Warning, "PARTIAL_FILE",
                  "Partial segment file has valid content but .partial extension", path);
        return SegmentStatus::Partial;
    }

    return SegmentStatus::ValidFinalized;
}

SegmentStatus validate_stream_set(const std::vector<std::string>& paths,
                                  std::vector<RawSegmentMetadata>& metas,
                                  std::vector<RawFooter>& footers,
                                  std::vector<RawValidationIssue>& issues,
                                  std::uint64_t* first_capture_utc_ns,
                                  std::uint64_t* last_capture_utc_ns,
                                  IFileSystem* fs) {
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
        std::uint64_t first_utc;
        std::uint64_t last_utc;
    };

    std::vector<SegmentEntry> entries;
    entries.reserve(paths.size());

    for (const auto& path : paths) {
        SegmentEntry entry;
        entry.path = path;
        entry.first_monotonic = 0;
        entry.last_monotonic = 0;
        entry.first_utc = 0;
        entry.last_utc = 0;

        // Parse canonical filename
        auto filename = std::filesystem::path(path).filename().string();
        if (!parse_canonical_filename(filename, entry.parsed_fn)) {
            add_issue(issues, ValidationSeverity::Error, "INVALID_FILENAME",
                      "Cannot parse canonical filename: " + filename, path);
            return SegmentStatus::Corrupt;
        }

        // Get file size via IFileSystem
        bool size_ok = false;
        entry.file_size = fs ? fs->file_size(path, size_ok) : [&]() {
            DefaultFileSystem dfs;
            return dfs.file_size(path, size_ok);
        }();
        if (!size_ok) {
            add_issue(issues, ValidationSeverity::Error, "IO_ERROR",
                      "Cannot get file size: " + path, path);
            return SegmentStatus::IoError;
        }

        // Validate the segment — collect first/last monotonic and UTC during the main pass
        std::vector<RawValidationIssue> seg_issues;
        auto status = validate_segment(path, entry.meta, entry.footer, seg_issues,
                                       entry.content_sha256_hex, entry.file_sha256_hex,
                                       &entry.first_monotonic, &entry.last_monotonic,
                                       &entry.first_utc, &entry.last_utc, fs);
        issues.insert(issues.end(), seg_issues.begin(), seg_issues.end());

        if (status != SegmentStatus::ValidFinalized) {
            add_issue(issues, ValidationSeverity::Error, "INVALID_SEGMENT",
                      "Segment not valid finalized: " + path, path);
            return status;
        }

        // Compare filename identity with content identity
        if (std::memcmp(entry.parsed_fn.session_id, entry.meta.session.session_id, 16) != 0) {
            add_issue(issues, ValidationSeverity::Error, "FILENAME_MISMATCH",
                      "Filename session_id does not match content: " + filename, path);
            return SegmentStatus::Corrupt;
        }
        if (entry.parsed_fn.source_id != entry.meta.source.source_id) {
            add_issue(issues, ValidationSeverity::Error, "FILENAME_MISMATCH",
                      "Filename source_id does not match content: " + filename, path);
            return SegmentStatus::Corrupt;
        }
        if (entry.parsed_fn.channel_id != entry.meta.source.channel_id) {
            add_issue(issues, ValidationSeverity::Error, "FILENAME_MISMATCH",
                      "Filename channel_id does not match content: " + filename, path);
            return SegmentStatus::Corrupt;
        }
        if (entry.parsed_fn.segment_index != entry.meta.segment_index) {
            add_issue(issues, ValidationSeverity::Error, "FILENAME_MISMATCH",
                      "Filename segment_index does not match content: " + filename, path);
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
        std::uint64_t expected_seg;
        if (!checked_add_u64(entries[i - 1].meta.segment_index, 1, expected_seg)) {
            add_issue(issues, ValidationSeverity::Error, "SEGMENT_INDEX_OVERFLOW",
                      "segment_index + 1 overflows u64");
            return SegmentStatus::Corrupt;
        }
        if (entries[i].meta.segment_index != expected_seg) {
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
        std::uint64_t expected_next_capture;
        if (!checked_add_u64(entries[i - 1].footer.last_capture_index, 1, expected_next_capture)) {
            add_issue(issues, ValidationSeverity::Error, "CAPTURE_INDEX_OVERFLOW",
                      "last_capture_index + 1 overflows u64");
            return SegmentStatus::Corrupt;
        }
        if (entries[i].footer.first_capture_index != expected_next_capture) {
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

    // Populate UTC bounds across all segments.
    // Entries are already sorted by segment index.
    // Select the FIRST non-zero UTC-valid timestamp and the LAST non-zero
    // UTC-valid timestamp in sorted segment/capture order — not numeric min/max.
    // Use 0/0 only when no UTC-valid records exist in any segment.
    std::uint64_t global_first_utc = 0;
    std::uint64_t global_last_utc = 0;
    for (const auto& e : entries) {
        if (e.first_utc != 0 && global_first_utc == 0) {
            global_first_utc = e.first_utc;
        }
        if (e.last_utc != 0) {
            global_last_utc = e.last_utc;
        }
    }
    if (first_capture_utc_ns) {
        *first_capture_utc_ns = global_first_utc;
    }
    if (last_capture_utc_ns) {
        *last_capture_utc_ns = global_last_utc;
    }

    return SegmentStatus::ValidFinalized;
}

std::vector<StreamSetInfo> group_stream_sets(const std::string& directory,
                                              std::vector<RawValidationIssue>& issues,
                                              IFileSystem* fs_arg) {
    auto* fs = fs_arg ? fs_arg : &g_default_fs;
    std::vector<StreamSetInfo> result;

    if (!std::filesystem::exists(directory)) return result;

    for (const auto& entry : std::filesystem::directory_iterator(directory)) {
        if (!entry.is_regular_file()) continue;

        auto filepath = entry.path().string();
        auto filename = entry.path().filename().string();

        // Handle .mxraw.partial files as partial candidates
        if (filename.size() > 14 && filename.substr(filename.size() - 14) == ".mxraw.partial") {
            add_issue(issues, ValidationSeverity::Warning, "PARTIAL_FILE",
                      "Partial segment file found: " + filepath, filepath);
            continue;
        }

        // Only process .mxraw files
        auto ext = entry.path().extension().string();
        if (ext != ".mxraw") continue;

        // Parse canonical filename first
        ParsedFilename parsed_fn;
        if (!parse_canonical_filename(filename, parsed_fn)) {
            add_issue(issues, ValidationSeverity::Error, "INVALID_FILENAME",
                      "Cannot parse canonical filename: " + filepath, filepath);
            continue;
        }

        // Read header to get session/source/channel (bounded: 4096 bytes)
        auto fh = fs->open_read(filepath);
        if (!fh) {
            add_issue(issues, ValidationSeverity::Error, "UNREADABLE_FILE",
                      "Cannot open .mxraw file: " + filepath, filepath);
            continue;
        }

        std::vector<std::uint8_t> header_data(kMaxHeaderSize);
        std::size_t n = fh->read(header_data.data(), kMaxHeaderSize);
        fh.reset();

        if (n == 0) {
            add_issue(issues, ValidationSeverity::Error, "EMPTY_FILE",
                      "Empty .mxraw file: " + filepath, filepath);
            continue;
        }

        RawSegmentMetadata meta;
        std::size_t header_size = 0;
        std::vector<RawValidationIssue> hdr_issues;

        if (!deserialize_header(header_data.data(), n, meta, header_size, hdr_issues)) {
            add_issue(issues, ValidationSeverity::Error, "MALFORMED_HEADER",
                      "Cannot parse .mxraw header: " + filepath, filepath);
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
