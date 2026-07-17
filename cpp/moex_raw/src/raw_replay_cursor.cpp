#include "moex_raw/raw_replay_cursor.hpp"
#include "moex_raw/raw_segment.hpp"
#include "moex_raw/endian.hpp"
#include "moex_raw/crc32c.hpp"
#include <cstring>
#include <filesystem>
#include <algorithm>

namespace moex_raw {

static void add_issue(std::vector<RawValidationIssue>& issues,
                      ValidationSeverity sev, const std::string& code,
                      const std::string& msg) {
    issues.push_back({sev, code, msg, {}, {}});
}

// --- ValidatedReplayCursor ---

ValidatedReplayCursor::ValidatedReplayCursor() = default;
ValidatedReplayCursor::~ValidatedReplayCursor() = default;

ValidatedReplayCursor::ValidatedReplayCursor(ValidatedReplayCursor&& other) noexcept
    : state_(other.state_)
    , terminal_code_(other.terminal_code_)
    , issues_(std::move(other.issues_))
    , preflight_(std::move(other.preflight_))
    , record_buf_(std::move(other.record_buf_))
    , current_segment_(other.current_segment_)
    , current_pos_(other.current_pos_)
    , data_end_(other.data_end_)
    , next_expected_capture_index_(other.next_expected_capture_index_)
    , last_capture_monotonic_ns_(other.last_capture_monotonic_ns_)
    , first_record_(other.first_record_)
    , ever_initialized_(other.ever_initialized_)
    , current_file_(std::move(other.current_file_))
    , default_fs_()
{
    if (other.fs_ == &other.default_fs_) {
        fs_ = &default_fs_;
    } else {
        fs_ = other.fs_;
    }
    other.state_ = ReplayCursorState::Uninitialized;
    other.terminal_code_ = ReplayCursorCode::Ok;
    other.fs_ = nullptr;
    other.ever_initialized_ = false;
    other.current_segment_ = 0;
    other.current_pos_ = 0;
    other.data_end_ = 0;
}

ValidatedReplayCursor& ValidatedReplayCursor::operator=(ValidatedReplayCursor&& other) noexcept {
    if (this != &other) {
        state_ = other.state_;
        terminal_code_ = other.terminal_code_;
        issues_ = std::move(other.issues_);
        preflight_ = std::move(other.preflight_);
        record_buf_ = std::move(other.record_buf_);
        current_segment_ = other.current_segment_;
        current_pos_ = other.current_pos_;
        data_end_ = other.data_end_;
        next_expected_capture_index_ = other.next_expected_capture_index_;
        last_capture_monotonic_ns_ = other.last_capture_monotonic_ns_;
        first_record_ = other.first_record_;
        ever_initialized_ = other.ever_initialized_;
        current_file_ = std::move(other.current_file_);

        if (other.fs_ == &other.default_fs_) {
            fs_ = &default_fs_;
        } else {
            fs_ = other.fs_;
        }

        other.state_ = ReplayCursorState::Uninitialized;
        other.terminal_code_ = ReplayCursorCode::Ok;
        other.fs_ = nullptr;
        other.ever_initialized_ = false;
        other.current_segment_ = 0;
        other.current_pos_ = 0;
        other.data_end_ = 0;
    }
    return *this;
}

void ValidatedReplayCursor::fail(ReplayCursorCode code,
                                 const std::string& issue_code,
                                 const std::string& msg) {
    state_ = ReplayCursorState::Failed;
    terminal_code_ = code;
    add_issue(issues_, ValidationSeverity::Error, issue_code, msg);
}

ReplayCursorInitResult ValidatedReplayCursor::initialize(
    const StreamSetInfo& stream_set, IFileSystem* fs) {

    // Safe defaults: fail-closed. All success paths set these explicitly.
    ReplayCursorInitResult result;
    result.code = ReplayCursorCode::ValidationFailed;
    result.segment_status = SegmentStatus::Corrupt;

    if (ever_initialized_) {
        result.code = ReplayCursorCode::AlreadyInitialized;
        result.segment_status = SegmentStatus::ValidFinalized;
        return result;
    }

    // Local issues — committed to issues_ only on full success.
    std::vector<RawValidationIssue> local_issues;
    IFileSystem* local_fs = fs ? fs : &default_fs_;

    // Build sorted path/index pairs
    if (stream_set.segment_paths.size() != stream_set.segment_indexes.size() ||
        stream_set.segment_paths.empty()) {
        add_issue(local_issues, ValidationSeverity::Error, "INVALID_STREAM_SET",
                  "StreamSetInfo paths/indexes mismatch or empty");
        result.issues = local_issues;
        return result;
    }

    std::vector<std::pair<std::string, std::uint64_t>> indexed;
    indexed.reserve(stream_set.segment_paths.size());
    for (std::size_t i = 0; i < stream_set.segment_paths.size(); ++i) {
        indexed.emplace_back(stream_set.segment_paths[i],
                             stream_set.segment_indexes[i]);
    }
    std::sort(indexed.begin(), indexed.end(),
              [](const auto& a, const auto& b) { return a.second < b.second; });

    std::vector<std::string> sorted_paths;
    sorted_paths.reserve(indexed.size());
    for (const auto& [p, idx] : indexed) {
        sorted_paths.push_back(p);
    }

    // Validate filename index matches declared index and identity matches StreamSetInfo
    for (std::size_t i = 0; i < indexed.size(); ++i) {
        ParsedFilename pf;
        auto fn = std::filesystem::path(indexed[i].first).filename().string();
        if (!parse_canonical_filename(fn, pf)) {
            add_issue(local_issues, ValidationSeverity::Error, "INVALID_FILENAME",
                      "Cannot parse canonical filename: " + fn);
            result.issues = local_issues;
            return result;
        }
        if (pf.segment_index != indexed[i].second) {
            add_issue(local_issues, ValidationSeverity::Error, "INDEX_MISMATCH",
                      "Filename segment_index does not match declared index");
            result.issues = local_issues;
            return result;
        }
        if (std::memcmp(pf.session_id, stream_set.session_id, 16) != 0) {
            add_issue(local_issues, ValidationSeverity::Error, "IDENTITY_MISMATCH",
                      "Filename session_id does not match StreamSetInfo");
            result.issues = local_issues;
            return result;
        }
        if (pf.source_id != stream_set.source_id) {
            add_issue(local_issues, ValidationSeverity::Error, "IDENTITY_MISMATCH",
                      "Filename source_id does not match StreamSetInfo");
            result.issues = local_issues;
            return result;
        }
        if (pf.channel_id != stream_set.channel_id) {
            add_issue(local_issues, ValidationSeverity::Error, "IDENTITY_MISMATCH",
                      "Filename channel_id does not match StreamSetInfo");
            result.issues = local_issues;
            return result;
        }
    }

    // Validate stream set
    std::vector<RawSegmentMetadata> metas;
    std::vector<RawFooter> footers;
    auto status = validate_stream_set(sorted_paths, metas, footers, local_issues,
                                      nullptr, nullptr, local_fs);

    if (status != SegmentStatus::ValidFinalized) {
        result.code = (status == SegmentStatus::IoError)
                          ? ReplayCursorCode::IoError
                          : ReplayCursorCode::ValidationFailed;
        result.segment_status = status;
        result.issues = local_issues;
        return result;
    }

    // Internal invariant check: sizes must be consistent and non-empty.
    // Access metas[0] only after this passes.
    if (metas.empty() || footers.size() != metas.size() ||
        metas.size() != indexed.size()) {
        add_issue(local_issues, ValidationSeverity::Error, "INTERNAL_INVARIANT_VIOLATION",
                  "validate_stream_set returned inconsistent metas/footers/indexed sizes");
        result.code = ReplayCursorCode::InternalInvariantViolation;
        result.segment_status = SegmentStatus::Corrupt;
        result.issues = local_issues;
        return result;
    }

    // Validate StreamSetInfo identity matches validated metadata
    if (std::memcmp(metas[0].session.session_id, stream_set.session_id, 16) != 0 ||
        metas[0].source.source_id != stream_set.source_id ||
        metas[0].source.channel_id != stream_set.channel_id) {
        add_issue(local_issues, ValidationSeverity::Error, "METADATA_IDENTITY_MISMATCH",
                  "Validated metadata does not match StreamSetInfo identity");
        result.issues = local_issues;
        return result;
    }

    // Build immutable preflight snapshots
    std::vector<SegmentPreflight> local_preflight;
    local_preflight.reserve(indexed.size());
    for (std::size_t i = 0; i < indexed.size(); ++i) {
        SegmentPreflight sp;
        sp.path = indexed[i].first;
        sp.segment_index = indexed[i].second;

        // Get file size
        bool size_ok = false;
        sp.file_size = local_fs->file_size(sp.path, size_ok);
        if (!size_ok) {
            add_issue(local_issues, ValidationSeverity::Error, "IO_ERROR",
                      "Cannot stat segment file during preflight");
            result.code = ReplayCursorCode::IoError;
            result.segment_status = SegmentStatus::IoError;
            result.issues = local_issues;
            return result;
        }

        // Get header size from file
        auto fh = local_fs->open_read(sp.path);
        if (!fh) {
            add_issue(local_issues, ValidationSeverity::Error, "IO_ERROR",
                      "Cannot open segment for preflight header read");
            result.code = ReplayCursorCode::IoError;
            result.segment_status = SegmentStatus::IoError;
            result.issues = local_issues;
            return result;
        }

        std::uint8_t preamble[14];
        if (fh->read(preamble, 14) != 14) {
            add_issue(local_issues, ValidationSeverity::Error, "IO_ERROR",
                      "Cannot read header preamble during preflight");
            result.code = ReplayCursorCode::IoError;
            result.segment_status = SegmentStatus::IoError;
            result.issues = local_issues;
            return result;
        }

        if (std::memcmp(preamble, kMagicRaw, 8) != 0) {
            add_issue(local_issues, ValidationSeverity::Error, "WRONG_MAGIC",
                      "Wrong segment magic during preflight");
            result.issues = local_issues;
            return result;
        }

        std::uint16_t version = read_u16_le(preamble + 8);
        if (version != kFormatVersion) {
            add_issue(local_issues, ValidationSeverity::Error, "UNSUPPORTED_VERSION",
                      "Unsupported format version during preflight");
            result.code = ReplayCursorCode::ValidationFailed;
            result.segment_status = SegmentStatus::Unsupported;
            result.issues = local_issues;
            return result;
        }

        std::size_t hsize = read_u32_le(preamble + 10);
        if (hsize > kMaxHeaderSize || hsize > sp.file_size) {
            add_issue(local_issues, ValidationSeverity::Error, "INVALID_HEADER_SIZE",
                      "Invalid header size during preflight");
            result.issues = local_issues;
            return result;
        }
        sp.header_size = hsize;

        // Store full metadata and footer from validation
        sp.metadata = metas[i];
        sp.footer = footers[i];

        // Verify metadata segment_index matches declared index
        if (metas[i].segment_index != indexed[i].second) {
            add_issue(local_issues, ValidationSeverity::Error, "SEGMENT_INDEX_MISMATCH",
                      "Validated segment_index does not match declared index");
            result.issues = local_issues;
            return result;
        }

        local_preflight.push_back(std::move(sp));
    }

    // --- Commit: all validation passed ---

    state_ = ReplayCursorState::Uninitialized;
    terminal_code_ = ReplayCursorCode::Ok;
    issues_ = std::move(local_issues);
    preflight_ = std::move(local_preflight);
    record_buf_.resize(static_cast<std::size_t>(kRecordHeaderSize) + kMaxPayloadSize + 4);
    current_segment_ = 0;
    current_pos_ = 0;
    data_end_ = 0;
    next_expected_capture_index_ = 0;
    last_capture_monotonic_ns_ = 0;
    first_record_ = true;
    current_file_.reset();
    fs_ = local_fs;

    state_ = ReplayCursorState::Ready;
    ever_initialized_ = true;
    result.code = ReplayCursorCode::Ok;
    result.segment_status = SegmentStatus::ValidFinalized;
    return result;
}

const RawSegmentMetadata* ValidatedReplayCursor::stream_metadata() const noexcept {
    if (preflight_.empty()) return nullptr;
    return &preflight_[0].metadata;
}

bool ValidatedReplayCursor::open_and_verify_segment(std::size_t idx) {
    const auto& pf = preflight_[idx];

    // Re-check file size
    bool size_ok = false;
    auto current_size = fs_->file_size(pf.path, size_ok);
    if (!size_ok) {
        fail(ReplayCursorCode::IoError, "IO_ERROR", "Cannot stat segment file");
        return false;
    }
    if (current_size != pf.file_size) {
        fail(ReplayCursorCode::StreamChanged, "STREAM_CHANGED",
             "Segment file size changed after preflight");
        return false;
    }

    // Open file
    current_file_ = fs_->open_read(pf.path);
    if (!current_file_) {
        fail(ReplayCursorCode::IoError, "IO_ERROR", "Cannot open segment file");
        return false;
    }

    // Re-read and validate header
    if (pf.header_size > 4096) {
        fail(ReplayCursorCode::StreamChanged, "STREAM_CHANGED",
             "Header size exceeds preflight");
        return false;
    }

    std::uint8_t hdr_buf[4096];
    if (!current_file_->seek(0, SEEK_SET)) {
        fail(ReplayCursorCode::IoError, "IO_ERROR", "Cannot seek to header start");
        current_file_.reset();
        return false;
    }
    if (current_file_->read(hdr_buf, pf.header_size) != pf.header_size) {
        fail(ReplayCursorCode::IoError, "IO_ERROR", "Cannot re-read segment header");
        current_file_.reset();
        return false;
    }

    RawSegmentMetadata re_meta;
    std::size_t re_header_size = 0;
    std::vector<RawValidationIssue> re_issues;
    if (!deserialize_header(hdr_buf, pf.header_size, re_meta, re_header_size, re_issues)) {
        fail(ReplayCursorCode::StreamChanged, "STREAM_CHANGED",
             "Segment header re-validation failed");
        current_file_.reset();
        return false;
    }

    // Compare full metadata with preflight snapshot
    if (re_header_size != pf.header_size ||
        std::memcmp(re_meta.session.session_id, pf.metadata.session.session_id, 16) != 0 ||
        re_meta.session.feed_group != pf.metadata.session.feed_group ||
        re_meta.session.endpoint_role != pf.metadata.session.endpoint_role ||
        re_meta.session.source_label != pf.metadata.session.source_label ||
        re_meta.source.clock_domain != pf.metadata.source.clock_domain ||
        re_meta.source.transport != pf.metadata.source.transport ||
        re_meta.source.source_side != pf.metadata.source.source_side ||
        re_meta.source.source_id != pf.metadata.source.source_id ||
        re_meta.source.channel_id != pf.metadata.source.channel_id ||
        std::memcmp(re_meta.source.configuration_sha256, pf.metadata.source.configuration_sha256, 32) != 0 ||
        std::memcmp(re_meta.source.templates_sha256, pf.metadata.source.templates_sha256, 32) != 0 ||
        std::memcmp(re_meta.source.endpoint_fingerprint_sha256, pf.metadata.source.endpoint_fingerprint_sha256, 32) != 0 ||
        re_meta.segment_index != pf.metadata.segment_index ||
        re_meta.start_capture_index != pf.metadata.start_capture_index ||
        re_meta.created_utc_ns != pf.metadata.created_utc_ns) {
        fail(ReplayCursorCode::StreamChanged, "STREAM_CHANGED",
             "Segment metadata mismatch after preflight");
        current_file_.reset();
        return false;
    }

    // Re-verify footer boundaries and fields
    if (pf.file_size < kFooterSize) {
        fail(ReplayCursorCode::StreamChanged, "STREAM_CHANGED",
             "File too small for footer");
        current_file_.reset();
        return false;
    }

    std::uint64_t footer_offset = pf.file_size - kFooterSize;
    if (!current_file_->seek(static_cast<std::int64_t>(footer_offset), SEEK_SET)) {
        fail(ReplayCursorCode::IoError, "IO_ERROR", "Cannot seek to footer");
        current_file_.reset();
        return false;
    }

    std::uint8_t footer_buf[92];
    if (current_file_->read(footer_buf, kFooterSize) != kFooterSize) {
        fail(ReplayCursorCode::IoError, "IO_ERROR", "Cannot read footer");
        current_file_.reset();
        return false;
    }

    RawFooter re_footer;
    std::vector<RawValidationIssue> footer_issues;
    if (!deserialize_footer(footer_buf, kFooterSize, re_footer, footer_issues)) {
        fail(ReplayCursorCode::StreamChanged, "STREAM_CHANGED",
             "Footer re-validation failed");
        current_file_.reset();
        return false;
    }

    if (re_footer.record_count != pf.footer.record_count ||
        re_footer.first_capture_index != pf.footer.first_capture_index ||
        re_footer.last_capture_index != pf.footer.last_capture_index ||
        re_footer.total_payload_bytes != pf.footer.total_payload_bytes ||
        re_footer.data_bytes_before_footer != pf.footer.data_bytes_before_footer ||
        std::memcmp(re_footer.content_sha256, pf.footer.content_sha256, 32) != 0) {
        fail(ReplayCursorCode::StreamChanged, "STREAM_CHANGED",
             "Footer fields mismatch after preflight");
        current_file_.reset();
        return false;
    }

    // Seek to data start
    current_pos_ = pf.header_size;
    data_end_ = pf.file_size - kFooterSize;

    if (!current_file_->seek(static_cast<std::int64_t>(current_pos_), SEEK_SET)) {
        fail(ReplayCursorCode::IoError, "IO_ERROR", "Cannot seek to data start");
        current_file_.reset();
        return false;
    }

    return true;
}

ReplayCursorResult ValidatedReplayCursor::next() {
    ReplayCursorResult result;

    if (state_ == ReplayCursorState::Uninitialized) {
        result.code = ReplayCursorCode::NotInitialized;
        return result;
    }

    if (state_ == ReplayCursorState::End) {
        result.code = ReplayCursorCode::End;
        return result;
    }

    if (state_ == ReplayCursorState::Failed) {
        result.code = terminal_code_;
        return result;
    }

    // State is Ready — read next record
    while (current_segment_ < preflight_.size()) {
        // Open segment file if needed
        if (!current_file_) {
            if (!open_and_verify_segment(current_segment_)) {
                result.code = terminal_code_;
                return result;
            }
        }

        if (current_pos_ >= data_end_) {
            // End of this segment; move to next
            current_file_.reset();
            current_segment_++;
            continue;
        }

        // Read record header
        std::uint8_t rec_hdr[kRecordHeaderSize];
        if (current_file_->read(rec_hdr, kRecordHeaderSize) != kRecordHeaderSize) {
            fail(ReplayCursorCode::IoError, "IO_ERROR", "Cannot read record header");
            current_file_.reset();
            result.code = terminal_code_;
            return result;
        }

        // Quick header validation for sizes
        if (std::memcmp(rec_hdr, kMagicRec, 4) != 0) {
            fail(ReplayCursorCode::StreamChanged, "WRONG_RECORD_MAGIC",
                 "Wrong record magic");
            current_file_.reset();
            result.code = terminal_code_;
            return result;
        }

        std::uint16_t hdr_size = read_u16_le(rec_hdr + 4);
        if (hdr_size != kRecordHeaderSize) {
            fail(ReplayCursorCode::StreamChanged, "WRONG_RECORD_HEADER_SIZE",
                 "Record header size not 44");
            current_file_.reset();
            result.code = terminal_code_;
            return result;
        }

        std::uint16_t flags = read_u16_le(rec_hdr + 6);
        if (flags & ~kRecordFlagUtcValid) {
            fail(ReplayCursorCode::StreamChanged, "UNKNOWN_RECORD_FLAG",
                 "Unknown record flag bit");
            current_file_.reset();
            result.code = terminal_code_;
            return result;
        }

        std::uint32_t record_size = read_u32_le(rec_hdr + 8);
        std::uint32_t payload_size = read_u32_le(rec_hdr + 36);

        if (payload_size > kMaxPayloadSize) {
            fail(ReplayCursorCode::StreamChanged, "PAYLOAD_TOO_LARGE",
                 "Payload exceeds 1 MiB");
            current_file_.reset();
            result.code = terminal_code_;
            return result;
        }

        std::uint32_t expected = kRecordHeaderSize + payload_size + 4;
        if (record_size != expected) {
            fail(ReplayCursorCode::StreamChanged, "WRONG_RECORD_SIZE",
                 "record_size mismatch");
            current_file_.reset();
            result.code = terminal_code_;
            return result;
        }

        // Bounds check
        if (current_pos_ + record_size > data_end_) {
            fail(ReplayCursorCode::StreamChanged, "RECORD_EXCEEDS_DATA",
                 "Record extends beyond data region");
            current_file_.reset();
            result.code = terminal_code_;
            return result;
        }

        // Read full record into bounded buffer
        std::size_t total = static_cast<std::size_t>(record_size);
        if (total > record_buf_.size()) {
            fail(ReplayCursorCode::StreamChanged, "RECORD_TOO_LARGE",
                 "Record exceeds bounded buffer");
            current_file_.reset();
            result.code = terminal_code_;
            return result;
        }

        std::memcpy(record_buf_.data(), rec_hdr, kRecordHeaderSize);
        std::size_t tail = total - kRecordHeaderSize;
        if (current_file_->read(record_buf_.data() + kRecordHeaderSize, tail) != tail) {
            fail(ReplayCursorCode::IoError, "IO_ERROR", "Cannot read record tail");
            current_file_.reset();
            result.code = terminal_code_;
            return result;
        }

        // Deserialize with full validation (including CRC)
        RawPacketRecordView view;
        std::size_t record_total_size = 0;
        std::vector<RawValidationIssue> rec_issues;
        if (!deserialize_record_view(record_buf_.data(), total, view,
                                     record_total_size, rec_issues)) {
            fail(ReplayCursorCode::StreamChanged, "RECORD_VALIDATION_FAILED",
                 "Record validation failed");
            issues_.insert(issues_.end(), rec_issues.begin(), rec_issues.end());
            current_file_.reset();
            result.code = terminal_code_;
            return result;
        }

        // Capture index continuity
        if (!first_record_) {
            if (view.capture_index != next_expected_capture_index_) {
                fail(ReplayCursorCode::StreamChanged, "INDEX_DISCONTINUITY",
                     "capture_index not continuous");
                current_file_.reset();
                result.code = terminal_code_;
                return result;
            }
        }

        // Non-decreasing capture_monotonic_ns
        if (!first_record_ && view.capture_monotonic_ns < last_capture_monotonic_ns_) {
            fail(ReplayCursorCode::StreamChanged, "MONOTONIC_VIOLATION",
                 "capture_monotonic_ns decreased");
            current_file_.reset();
            result.code = terminal_code_;
            return result;
        }

        // Advance state
        next_expected_capture_index_ = view.capture_index + 1;
        last_capture_monotonic_ns_ = view.capture_monotonic_ns;
        first_record_ = false;
        current_pos_ += record_size;

        if (!current_file_->seek(static_cast<std::int64_t>(current_pos_), SEEK_SET)) {
            fail(ReplayCursorCode::IoError, "IO_ERROR", "Cannot seek to next record");
            current_file_.reset();
            result.code = terminal_code_;
            return result;
        }

        // Return borrowed view — payload points into record_buf_
        result.code = ReplayCursorCode::Ok;
        result.record = view;
        return result;
    }

    // All segments exhausted
    state_ = ReplayCursorState::End;
    result.code = ReplayCursorCode::End;
    return result;
}

}  // namespace moex_raw
