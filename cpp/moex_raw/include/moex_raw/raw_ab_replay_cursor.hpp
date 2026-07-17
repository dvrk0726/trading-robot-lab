#pragma once
#include "moex_raw/raw_types.hpp"
#include "moex_raw/raw_segment.hpp"
#include <cstdint>
#include <memory>
#include <vector>

namespace moex_raw {

enum class ClockMergeContract : std::uint8_t {
    Unspecified = 0,
    SharedMonotonicTimeline = 1
};

enum class AbReplayState : std::uint8_t {
    Uninitialized = 0,
    Ready = 1,
    End = 2,
    Failed = 3
};

enum class AbReplayCode : std::uint8_t {
    Ok = 0,
    End = 1,
    NotInitialized = 2,
    AlreadyInitialized = 3,
    ValidationFailed = 4,
    IoError = 5,
    StreamChanged = 6,
    ClockRegression = 7,
    InternalInvariantViolation = 8
};

struct AbReplayInitResult {
    AbReplayCode code = AbReplayCode::ValidationFailed;
    SegmentStatus a_status = SegmentStatus::Corrupt;
    SegmentStatus b_status = SegmentStatus::Corrupt;
    std::vector<RawValidationIssue> issues;
};

struct AbReplayResult {
    AbReplayCode code = AbReplayCode::NotInitialized;
    SourceSide source_side = SourceSide::None;
    RawPacketRecordView record;
};

class ValidatedAbReplayCursor {
public:
    ValidatedAbReplayCursor();
    ~ValidatedAbReplayCursor();
    ValidatedAbReplayCursor(ValidatedAbReplayCursor&&) noexcept;
    ValidatedAbReplayCursor& operator=(ValidatedAbReplayCursor&&) noexcept;
    ValidatedAbReplayCursor(const ValidatedAbReplayCursor&) = delete;
    ValidatedAbReplayCursor& operator=(const ValidatedAbReplayCursor&) = delete;

    AbReplayState state() const noexcept;
    AbReplayCode terminal_code() const noexcept;
    const std::vector<RawValidationIssue>& issues() const;

    AbReplayInitResult initialize(
        const StreamSetInfo& first,
        const StreamSetInfo& second,
        ClockMergeContract clock_contract,
        IFileSystem* first_fs = nullptr,
        IFileSystem* second_fs = nullptr);

    AbReplayResult next();

    const RawSegmentMetadata* metadata(SourceSide side) const noexcept;

private:
    friend struct ValidatedAbReplayCursorTestAccess;
    static AbReplayCode classify_child_init_code(ReplayCursorCode code);
    static AbReplayCode classify_child_code(ReplayCursorCode code);
    static bool is_valid_initial_code(ReplayCursorCode code);

    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace moex_raw
