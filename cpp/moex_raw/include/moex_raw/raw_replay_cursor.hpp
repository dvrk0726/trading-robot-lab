#pragma once
#include "moex_raw/raw_types.hpp"
#include "moex_raw/raw_segment.hpp"
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace moex_raw {

// ValidatedReplayCursor: streaming replay of one RT-2 StreamSetInfo.
// States: Uninitialized, Ready, End, Failed.
class ValidatedReplayCursor {
public:
    ValidatedReplayCursor();
    ~ValidatedReplayCursor();
    ValidatedReplayCursor(ValidatedReplayCursor&&) noexcept;
    ValidatedReplayCursor& operator=(ValidatedReplayCursor&&) noexcept;
    ValidatedReplayCursor(const ValidatedReplayCursor&) = delete;
    ValidatedReplayCursor& operator=(const ValidatedReplayCursor&) = delete;

    ReplayCursorState state() const { return state_; }
    const std::vector<RawValidationIssue>& issues() const { return issues_; }
    ReplayCursorCode terminal_code() const { return terminal_code_; }

    // Validate stream set, sort paths by segment_index, store preflight
    // snapshot (path, file_size, header_size, full metadata, footer) per
    // segment, allocate bounded record buffer.
    // After success: AlreadyInitialized on repeat call (position unchanged).
    // After failure: retry allowed.
    ReplayCursorInitResult initialize(const StreamSetInfo& stream_set,
                                      IFileSystem* fs = nullptr);

    // Read one record. Returns borrowed view with payload valid only
    // until next next(), move, or destruction.
    // On NotInitialized/End/Failed: record is deterministically empty.
    ReplayCursorResult next();

private:
    struct SegmentPreflight {
        std::string path;
        std::uint64_t segment_index = 0;
        std::uint64_t file_size = 0;
        std::size_t header_size = 0;
        RawSegmentMetadata metadata;
        RawFooter footer;
    };

    void fail(ReplayCursorCode code, const std::string& issue_code,
              const std::string& msg);
    bool open_and_verify_segment(std::size_t idx);

    ReplayCursorState state_ = ReplayCursorState::Uninitialized;
    ReplayCursorCode terminal_code_ = ReplayCursorCode::Ok;
    std::vector<RawValidationIssue> issues_;
    std::vector<SegmentPreflight> preflight_;
    std::vector<std::uint8_t> record_buf_;
    std::size_t current_segment_ = 0;
    std::uint64_t current_pos_ = 0;
    std::uint64_t data_end_ = 0;
    std::uint64_t next_expected_capture_index_ = 0;
    std::uint64_t last_capture_monotonic_ns_ = 0;
    bool first_record_ = true;
    bool ever_initialized_ = false;
    std::unique_ptr<IFileHandle> current_file_;
    IFileSystem* fs_ = nullptr;
    DefaultFileSystem default_fs_;
};

}  // namespace moex_raw
