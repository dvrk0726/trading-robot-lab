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
ValidatedReplayCursor::ValidatedReplayCursor(ValidatedReplayCursor&&) noexcept = default;
ValidatedReplayCursor& ValidatedReplayCursor::operator=(ValidatedReplayCursor&&) noexcept = default;

ValidatedReplayCursor::InitStatus ValidatedReplayCursor::initialize(
    const std::vector<std::string>& paths) {

    if (state_ == State::Ready) {
        return InitStatus::AlreadyInitialized;
    }

    // Reset for fresh attempt (covers Uninitialized and previous Failed)
    state_ = State::Uninitialized;
    issues_.clear();
    segments_.clear();
    current_segment_ = 0;
    current_pos_ = 0;
    data_end_ = 0;
    next_expected_capture_index_ = 0;
    last_capture_monotonic_ns_ = 0;
    first_record_ = true;
    current_file_.reset();

    fs_ = &default_fs_;

    // Validate stream set (sorts by numeric segment_index internally)
    std::vector<RawSegmentMetadata> metas;
    std::vector<RawFooter> footers;
    auto status = validate_stream_set(paths, metas, footers, issues_, nullptr, nullptr, fs_);

    if (status != SegmentStatus::ValidFinalized) {
        return InitStatus::Failed;
    }

    if (metas.empty()) {
        add_issue(issues_, ValidationSeverity::Error, "EMPTY_STREAM", "No segments in stream set");
        return InitStatus::Failed;
    }

    // Sort paths by numeric segment_index
    std::vector<std::pair<std::string, std::uint64_t>> indexed;
    indexed.reserve(paths.size());
    for (const auto& p : paths) {
        ParsedFilename pf;
        auto fn = std::filesystem::path(p).filename().string();
        if (!parse_canonical_filename(fn, pf)) {
            add_issue(issues_, ValidationSeverity::Error, "INVALID_FILENAME", "Cannot parse segment filename");
            return InitStatus::Failed;
        }
        indexed.emplace_back(p, pf.segment_index);
    }
    std::sort(indexed.begin(), indexed.end(),
              [](const auto& a, const auto& b) { return a.second < b.second; });

    // Build segment info and store preflight state
    segments_.reserve(indexed.size());
    for (std::size_t i = 0; i < indexed.size(); ++i) {
        SegmentInfo si;
        si.path = indexed[i].first;
        si.segment_index = indexed[i].second;

        // Validate and cache header_size + file_size
        if (!parse_segment_header(si.path, si)) {
            return InitStatus::Failed;
        }
        segments_.push_back(si);
    }

    // Store validated metadata from first segment (already sorted)
    validated_meta_ = metas[0];

    // Allocate bounded record buffer (once)
    record_buf_.resize(static_cast<std::size_t>(kRecordHeaderSize) + kMaxPayloadSize + 4);

    state_ = State::Ready;
    return InitStatus::Ok;
}

bool ValidatedReplayCursor::next(RawPacketRecord& out) {
    if (state_ == State::End || state_ == State::Failed || state_ == State::Uninitialized) {
        return false;
    }

    while (current_segment_ < segments_.size()) {
        // Open segment file if needed
        if (!current_file_) {
            current_file_ = fs_->open_read(segments_[current_segment_].path);
            if (!current_file_) {
                state_ = State::Failed;
                error_code_ = CursorErrorCode::IoError;
                add_issue(issues_, ValidationSeverity::Error, "IO_ERROR", "Cannot open segment file");
                return false;
            }

            // Re-check file size against preflight
            bool size_ok = false;
            auto current_size = fs_->file_size(segments_[current_segment_].path, size_ok);
            if (!size_ok) {
                state_ = State::Failed;
                error_code_ = CursorErrorCode::IoError;
                add_issue(issues_, ValidationSeverity::Error, "IO_ERROR", "Cannot stat segment file");
                current_file_.reset();
                return false;
            }
            if (current_size != segments_[current_segment_].file_size) {
                state_ = State::Failed;
                error_code_ = CursorErrorCode::StreamChanged;
                add_issue(issues_, ValidationSeverity::Error, "STREAM_CHANGED", "Segment file size changed after preflight");
                current_file_.reset();
                return false;
            }

            // Re-read and validate header, compare with stored metadata
            if (!parse_segment_header(segments_[current_segment_].path, segments_[current_segment_])) {
                state_ = State::Failed;
                error_code_ = CursorErrorCode::StreamChanged;
                current_file_.reset();
                return false;
            }

            // Re-validate full segment header against stored metadata
            std::uint8_t hdr_buf[4096];
            std::size_t to_read = segments_[current_segment_].header_size;
            if (to_read > 4096) {
                state_ = State::Failed;
                error_code_ = CursorErrorCode::StreamChanged;
                add_issue(issues_, ValidationSeverity::Error, "STREAM_CHANGED", "Header size exceeds preflight");
                current_file_.reset();
                return false;
            }
            if (!current_file_->seek(0, SEEK_SET)) {
                state_ = State::Failed;
                error_code_ = CursorErrorCode::IoError;
                add_issue(issues_, ValidationSeverity::Error, "IO_ERROR", "Cannot seek to header start");
                current_file_.reset();
                return false;
            }
            if (current_file_->read(hdr_buf, to_read) != to_read) {
                state_ = State::Failed;
                error_code_ = CursorErrorCode::IoError;
                add_issue(issues_, ValidationSeverity::Error, "IO_ERROR", "Cannot re-read segment header");
                current_file_.reset();
                return false;
            }

            RawSegmentMetadata re_meta;
            std::size_t re_header_size = 0;
            std::vector<RawValidationIssue> re_issues;
            if (!deserialize_header(hdr_buf, to_read, re_meta, re_header_size, re_issues)) {
                state_ = State::Failed;
                error_code_ = CursorErrorCode::StreamChanged;
                add_issue(issues_, ValidationSeverity::Error, "STREAM_CHANGED", "Segment header re-validation failed");
                current_file_.reset();
                return false;
            }

            // Compare critical metadata fields
            if (std::memcmp(re_meta.session.session_id, validated_meta_.session.session_id, 16) != 0 ||
                re_meta.source.source_id != validated_meta_.source.source_id ||
                re_meta.source.channel_id != validated_meta_.source.channel_id ||
                re_meta.segment_index != segments_[current_segment_].segment_index) {
                state_ = State::Failed;
                error_code_ = CursorErrorCode::StreamChanged;
                add_issue(issues_, ValidationSeverity::Error, "STREAM_CHANGED", "Segment metadata mismatch after preflight");
                current_file_.reset();
                return false;
            }

            // Seek to data start
            current_pos_ = segments_[current_segment_].header_size;
            data_end_ = segments_[current_segment_].file_size - kFooterSize;

            if (!current_file_->seek(static_cast<std::int64_t>(current_pos_), SEEK_SET)) {
                state_ = State::Failed;
                error_code_ = CursorErrorCode::IoError;
                add_issue(issues_, ValidationSeverity::Error, "IO_ERROR", "Cannot seek to data start");
                current_file_.reset();
                return false;
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
            state_ = State::Failed;
            error_code_ = CursorErrorCode::IoError;
            add_issue(issues_, ValidationSeverity::Error, "IO_ERROR", "Cannot read record header");
            current_file_.reset();
            return false;
        }

        // Quick header validation for sizes
        std::uint32_t record_size = 0;
        std::uint32_t payload_size = 0;
        if (!validate_record_header_only(rec_hdr, kRecordHeaderSize, record_size, payload_size)) {
            state_ = State::Failed;
            error_code_ = CursorErrorCode::StreamChanged;
            current_file_.reset();
            return false;
        }

        // Bounds check
        if (current_pos_ + record_size > data_end_) {
            state_ = State::Failed;
            error_code_ = CursorErrorCode::StreamChanged;
            add_issue(issues_, ValidationSeverity::Error, "RECORD_EXCEEDS_DATA", "Record extends beyond data region");
            current_file_.reset();
            return false;
        }

        // Read full record into bounded buffer
        std::size_t total = static_cast<std::size_t>(record_size);
        if (total > record_buf_.size()) {
            state_ = State::Failed;
            error_code_ = CursorErrorCode::StreamChanged;
            add_issue(issues_, ValidationSeverity::Error, "RECORD_TOO_LARGE", "Record exceeds bounded buffer");
            current_file_.reset();
            return false;
        }

        std::memcpy(record_buf_.data(), rec_hdr, kRecordHeaderSize);
        std::size_t tail = total - kRecordHeaderSize;
        if (current_file_->read(record_buf_.data() + kRecordHeaderSize, tail) != tail) {
            state_ = State::Failed;
            error_code_ = CursorErrorCode::IoError;
            add_issue(issues_, ValidationSeverity::Error, "IO_ERROR", "Cannot read record tail");
            current_file_.reset();
            return false;
        }

        // Deserialize with full validation (including CRC)
        RawPacketRecordView view;
        std::size_t record_total_size = 0;
        std::vector<RawValidationIssue> rec_issues;
        if (!deserialize_record_view(record_buf_.data(), total, view, record_total_size, rec_issues)) {
            state_ = State::Failed;
            error_code_ = CursorErrorCode::StreamChanged;
            issues_.insert(issues_.end(), rec_issues.begin(), rec_issues.end());
            current_file_.reset();
            return false;
        }

        // Capture index continuity
        if (!first_record_) {
            if (view.capture_index != next_expected_capture_index_) {
                state_ = State::Failed;
                error_code_ = CursorErrorCode::StreamChanged;
                add_issue(issues_, ValidationSeverity::Error, "INDEX_DISCONTINUITY",
                          "capture_index not continuous");
                current_file_.reset();
                return false;
            }
        }

        // Non-decreasing capture_monotonic_ns
        if (!first_record_ && view.capture_monotonic_ns < last_capture_monotonic_ns_) {
            state_ = State::Failed;
            error_code_ = CursorErrorCode::StreamChanged;
            add_issue(issues_, ValidationSeverity::Error, "MONOTONIC_VIOLATION",
                      "capture_monotonic_ns decreased");
            current_file_.reset();
            return false;
        }

        // Populate owning output
        out.record_flags = view.record_flags;
        out.capture_index = view.capture_index;
        out.capture_utc_ns = view.capture_utc_ns;
        out.capture_monotonic_ns = view.capture_monotonic_ns;
        out.payload.assign(view.payload.begin(), view.payload.end());

        // Advance state
        next_expected_capture_index_ = view.capture_index + 1;
        last_capture_monotonic_ns_ = view.capture_monotonic_ns;
        first_record_ = false;
        current_pos_ += record_size;

        if (!current_file_->seek(static_cast<std::int64_t>(current_pos_), SEEK_SET)) {
            state_ = State::Failed;
            error_code_ = CursorErrorCode::IoError;
            add_issue(issues_, ValidationSeverity::Error, "IO_ERROR", "Cannot seek to next record");
            current_file_.reset();
            return false;
        }

        return true;
    }

    // All segments exhausted
    state_ = State::End;
    return false;
}

bool ValidatedReplayCursor::parse_segment_header(const std::string& path, SegmentInfo& info) {
    bool size_ok = false;
    auto fsize = fs_->file_size(path, size_ok);
    if (!size_ok) {
        add_issue(issues_, ValidationSeverity::Error, "IO_ERROR", "Cannot stat segment file");
        return false;
    }
    info.file_size = fsize;

    auto fh = fs_->open_read(path);
    if (!fh) {
        add_issue(issues_, ValidationSeverity::Error, "IO_ERROR", "Cannot open segment for header read");
        return false;
    }

    std::uint8_t preamble[14];
    if (fh->read(preamble, 14) != 14) {
        add_issue(issues_, ValidationSeverity::Error, "IO_ERROR", "Cannot read header preamble");
        return false;
    }

    if (std::memcmp(preamble, kMagicRaw, 8) != 0) {
        add_issue(issues_, ValidationSeverity::Error, "WRONG_MAGIC", "Wrong segment magic");
        return false;
    }

    std::uint16_t version = read_u16_le(preamble + 8);
    if (version != kFormatVersion) {
        add_issue(issues_, ValidationSeverity::Error, "UNSUPPORTED_VERSION", "Unsupported format version");
        return false;
    }

    std::size_t hsize = read_u32_le(preamble + 10);
    if (hsize > kMaxHeaderSize || hsize > fsize) {
        add_issue(issues_, ValidationSeverity::Error, "INVALID_HEADER_SIZE", "Invalid header size");
        return false;
    }

    info.header_size = hsize;
    return true;
}

bool ValidatedReplayCursor::validate_record_header_only(
    const std::uint8_t* data, std::size_t available,
    std::uint32_t& record_size, std::uint32_t& payload_size) {

    if (available < kRecordHeaderSize) {
        add_issue(issues_, ValidationSeverity::Error, "TRUNCATED_RECORD", "Truncated record header");
        return false;
    }

    if (std::memcmp(data, kMagicRec, 4) != 0) {
        add_issue(issues_, ValidationSeverity::Error, "WRONG_RECORD_MAGIC", "Wrong record magic");
        return false;
    }

    std::uint16_t hdr_size = read_u16_le(data + 4);
    if (hdr_size != kRecordHeaderSize) {
        add_issue(issues_, ValidationSeverity::Error, "WRONG_RECORD_HEADER_SIZE", "Record header size not 44");
        return false;
    }

    std::uint16_t flags = read_u16_le(data + 6);
    if (flags & ~kRecordFlagUtcValid) {
        add_issue(issues_, ValidationSeverity::Error, "UNKNOWN_RECORD_FLAG", "Unknown record flag bit");
        return false;
    }

    record_size = read_u32_le(data + 8);
    payload_size = read_u32_le(data + 36);

    if (payload_size > kMaxPayloadSize) {
        add_issue(issues_, ValidationSeverity::Error, "PAYLOAD_TOO_LARGE", "Payload exceeds 1 MiB");
        return false;
    }

    std::uint32_t expected = kRecordHeaderSize + payload_size + 4;
    if (record_size != expected) {
        add_issue(issues_, ValidationSeverity::Error, "WRONG_RECORD_SIZE", "record_size mismatch");
        return false;
    }

    return true;
}

}  // namespace moex_raw
