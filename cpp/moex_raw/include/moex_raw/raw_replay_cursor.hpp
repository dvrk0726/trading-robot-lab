#pragma once
#include "moex_raw/raw_types.hpp"
#include "moex_raw/raw_segment.hpp"
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace moex_raw {

// Cursor-level error codes.
enum class CursorErrorCode {
    StreamChanged,
    IoError
};

// ValidatedReplayCursor: streaming replay of one RT-2 StreamSetInfo.
// States: Uninitialized, Ready, End, Failed.
class ValidatedReplayCursor {
public:
    enum class State { Uninitialized, Ready, End, Failed };

    ValidatedReplayCursor();
    ~ValidatedReplayCursor();
    ValidatedReplayCursor(ValidatedReplayCursor&&) noexcept;
    ValidatedReplayCursor& operator=(ValidatedReplayCursor&&) noexcept;
    ValidatedReplayCursor(const ValidatedReplayCursor&) = delete;
    ValidatedReplayCursor& operator=(const ValidatedReplayCursor&) = delete;

    State state() const { return state_; }
    const std::vector<RawValidationIssue>& issues() const { return issues_; }
    CursorErrorCode error_code() const { return error_code_; }
    const RawSegmentMetadata& validated_metadata() const { return validated_meta_; }

    enum class InitStatus { Ok, AlreadyInitialized, Failed };

    // Validate stream set, sort paths by segment_index, store validated
    // metadata, allocate bounded record buffer.
    // After success: AlreadyInitialized on repeat call (position unchanged).
    // After failure: retry allowed.
    InitStatus initialize(const std::vector<std::string>& paths);

    // Read one record. Returns borrowed view with payload valid only
    // until next next(), move, or destruction.
    bool next(RawPacketRecord& out);

private:
    struct SegmentInfo {
        std::string path;
        std::uint64_t segment_index = 0;
        std::size_t header_size = 0;
        std::uint64_t file_size = 0;
    };

    bool parse_segment_header(const std::string& path, SegmentInfo& info);
    bool validate_record_header_only(const std::uint8_t* data, std::size_t available,
                                     std::uint32_t& record_size, std::uint32_t& payload_size);

    State state_ = State::Uninitialized;
    CursorErrorCode error_code_ = CursorErrorCode::StreamChanged;
    std::vector<RawValidationIssue> issues_;
    RawSegmentMetadata validated_meta_;
    std::vector<SegmentInfo> segments_;
    std::vector<std::uint8_t> record_buf_;
    std::size_t current_segment_ = 0;
    std::uint64_t current_pos_ = 0;
    std::uint64_t data_end_ = 0;
    std::uint64_t next_expected_capture_index_ = 0;
    std::uint64_t last_capture_monotonic_ns_ = 0;
    bool first_record_ = true;
    std::unique_ptr<IFileHandle> current_file_;
    IFileSystem* fs_ = nullptr;
    DefaultFileSystem default_fs_;
};

}  // namespace moex_raw
