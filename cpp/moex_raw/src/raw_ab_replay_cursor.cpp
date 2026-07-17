#include "moex_raw/raw_ab_replay_cursor.hpp"
#include "moex_raw/raw_replay_cursor.hpp"
#include <cstring>
#include <tuple>

namespace moex_raw {

static AbReplayCode map_child_code(ReplayCursorCode code) {
    switch (code) {
        case ReplayCursorCode::IoError: return AbReplayCode::IoError;
        case ReplayCursorCode::StreamChanged: return AbReplayCode::StreamChanged;
        case ReplayCursorCode::InternalInvariantViolation: return AbReplayCode::InternalInvariantViolation;
        default: return AbReplayCode::InternalInvariantViolation;
    }
}

// Merge key: (capture_monotonic_ns, side_rank, capture_index)
// side_rank: A=0, B=1
using MergeKey = std::tuple<std::uint64_t, std::uint8_t, std::uint64_t>;

static MergeKey make_merge_key(const RawPacketRecordView& rec, SourceSide side) {
    return std::make_tuple(
        rec.capture_monotonic_ns,
        static_cast<std::uint8_t>(side == SourceSide::A ? 0 : 1),
        rec.capture_index);
}

struct ValidatedAbReplayCursor::Impl {
    AbReplayState state = AbReplayState::Uninitialized;
    AbReplayCode terminal_code = AbReplayCode::Ok;
    std::vector<RawValidationIssue> issues;

    std::unique_ptr<ValidatedReplayCursor> cursor_a;
    std::unique_ptr<ValidatedReplayCursor> cursor_b;

    ReplayCursorResult lookahead_a;
    ReplayCursorResult lookahead_b;
    bool a_alive = false;
    bool b_alive = false;
    bool a_end = false;
    bool b_end = false;

    RawSegmentMetadata meta_a;
    RawSegmentMetadata meta_b;
    bool has_meta_a = false;
    bool has_meta_b = false;

    SourceSide last_issued = SourceSide::None;
    bool has_last_key = false;
    MergeKey last_key;

    bool ever_initialized = false;

    void fail(AbReplayCode code, const std::string& issue_code,
              const std::string& msg) {
        state = AbReplayState::Failed;
        terminal_code = code;
        issues.push_back({ValidationSeverity::Error, issue_code, msg, {}, {}});
    }
};

ValidatedAbReplayCursor::ValidatedAbReplayCursor() : impl_(std::make_unique<Impl>()) {}
ValidatedAbReplayCursor::~ValidatedAbReplayCursor() = default;

ValidatedAbReplayCursor::ValidatedAbReplayCursor(ValidatedAbReplayCursor&&) noexcept = default;
ValidatedAbReplayCursor& ValidatedAbReplayCursor::operator=(ValidatedAbReplayCursor&&) noexcept = default;

AbReplayState ValidatedAbReplayCursor::state() const noexcept { return impl_->state; }
AbReplayCode ValidatedAbReplayCursor::terminal_code() const noexcept { return impl_->terminal_code; }
const std::vector<RawValidationIssue>& ValidatedAbReplayCursor::issues() const { return impl_->issues; }

const RawSegmentMetadata* ValidatedAbReplayCursor::metadata(SourceSide side) const noexcept {
    if (side == SourceSide::A && impl_->has_meta_a) return &impl_->meta_a;
    if (side == SourceSide::B && impl_->has_meta_b) return &impl_->meta_b;
    return nullptr;
}

AbReplayInitResult ValidatedAbReplayCursor::initialize(
    const StreamSetInfo& first,
    const StreamSetInfo& second,
    ClockMergeContract clock_contract,
    IFileSystem* first_fs,
    IFileSystem* second_fs) {

    AbReplayInitResult result;
    result.code = AbReplayCode::ValidationFailed;
    result.a_status = SegmentStatus::Corrupt;
    result.b_status = SegmentStatus::Corrupt;

    if (impl_->ever_initialized) {
        result.code = AbReplayCode::AlreadyInitialized;
        result.a_status = SegmentStatus::ValidFinalized;
        result.b_status = SegmentStatus::ValidFinalized;
        return result;
    }

    if (clock_contract != ClockMergeContract::SharedMonotonicTimeline) {
        result.issues.push_back({ValidationSeverity::Error, "INVALID_CLOCK_CONTRACT",
                                 "Clock contract must be SharedMonotonicTimeline", {}, {}});
        return result;
    }

    ValidatedReplayCursor local_first;
    ValidatedReplayCursor local_second;

    auto init_first = local_first.initialize(first, first_fs);
    if (init_first.code != ReplayCursorCode::Ok) {
        result.code = (init_first.code == ReplayCursorCode::IoError)
                          ? AbReplayCode::IoError
                          : AbReplayCode::ValidationFailed;
        result.issues = init_first.issues;
        return result;
    }

    auto init_second = local_second.initialize(second, second_fs);
    if (init_second.code != ReplayCursorCode::Ok) {
        result.code = (init_second.code == ReplayCursorCode::IoError)
                          ? AbReplayCode::IoError
                          : AbReplayCode::ValidationFailed;
        result.issues = init_second.issues;
        return result;
    }

    const auto* meta_first = local_first.stream_metadata();
    const auto* meta_second = local_second.stream_metadata();
    if (!meta_first || !meta_second) {
        result.code = AbReplayCode::InternalInvariantViolation;
        result.issues.push_back({ValidationSeverity::Error, "MISSING_METADATA",
                                 "stream_metadata null after successful initialize", {}, {}});
        return result;
    }

    // Determine A/B by validated stream_metadata independently of argument order
    const RawSegmentMetadata* meta_a_ptr = nullptr;
    const RawSegmentMetadata* meta_b_ptr = nullptr;
    ValidatedReplayCursor* cursor_a_ptr = nullptr;
    ValidatedReplayCursor* cursor_b_ptr = nullptr;
    SegmentStatus first_status = init_first.segment_status;
    SegmentStatus second_status = init_second.segment_status;
    SegmentStatus status_a = SegmentStatus::Corrupt;
    SegmentStatus status_b = SegmentStatus::Corrupt;

    if (meta_first->source.source_side == SourceSide::A &&
        meta_second->source.source_side == SourceSide::B) {
        meta_a_ptr = meta_first;
        meta_b_ptr = meta_second;
        cursor_a_ptr = &local_first;
        cursor_b_ptr = &local_second;
        status_a = first_status;
        status_b = second_status;
    } else if (meta_first->source.source_side == SourceSide::B &&
               meta_second->source.source_side == SourceSide::A) {
        meta_a_ptr = meta_second;
        meta_b_ptr = meta_first;
        cursor_a_ptr = &local_second;
        cursor_b_ptr = &local_first;
        status_a = second_status;
        status_b = first_status;
    } else {
        result.code = AbReplayCode::ValidationFailed;
        result.issues.push_back({ValidationSeverity::Error, "SIDE_MISMATCH",
                                 "Must have exactly one SourceSide::A and one SourceSide::B", {}, {}});
        return result;
    }

    result.a_status = status_a;
    result.b_status = status_b;

    // Both Transport::Udp
    if (meta_a_ptr->source.transport != Transport::Udp ||
        meta_b_ptr->source.transport != Transport::Udp) {
        result.code = AbReplayCode::ValidationFailed;
        result.issues.push_back({ValidationSeverity::Error, "TRANSPORT_MISMATCH",
                                 "Both streams must use Transport::Udp", {}, {}});
        return result;
    }

    // session_id match
    if (std::memcmp(meta_a_ptr->session.session_id, meta_b_ptr->session.session_id, 16) != 0) {
        result.code = AbReplayCode::ValidationFailed;
        result.issues.push_back({ValidationSeverity::Error, "SESSION_ID_MISMATCH",
                                 "session_id must match between A and B", {}, {}});
        return result;
    }

    // feed_group match
    if (meta_a_ptr->session.feed_group != meta_b_ptr->session.feed_group) {
        result.code = AbReplayCode::ValidationFailed;
        result.issues.push_back({ValidationSeverity::Error, "FEED_GROUP_MISMATCH",
                                 "feed_group must match between A and B", {}, {}});
        return result;
    }

    // configuration_sha256 match
    if (std::memcmp(meta_a_ptr->source.configuration_sha256, meta_b_ptr->source.configuration_sha256, 32) != 0) {
        result.code = AbReplayCode::ValidationFailed;
        result.issues.push_back({ValidationSeverity::Error, "CONFIG_SHA256_MISMATCH",
                                 "configuration_sha256 must match between A and B", {}, {}});
        return result;
    }

    // templates_sha256 match
    if (std::memcmp(meta_a_ptr->source.templates_sha256, meta_b_ptr->source.templates_sha256, 32) != 0) {
        result.code = AbReplayCode::ValidationFailed;
        result.issues.push_back({ValidationSeverity::Error, "TEMPLATES_SHA256_MISMATCH",
                                 "templates_sha256 must match between A and B", {}, {}});
        return result;
    }

    // clock_domain match
    if (meta_a_ptr->source.clock_domain != meta_b_ptr->source.clock_domain) {
        result.code = AbReplayCode::ValidationFailed;
        result.issues.push_back({ValidationSeverity::Error, "CLOCK_DOMAIN_MISMATCH",
                                 "clock_domain must match between A and B", {}, {}});
        return result;
    }

    // All metadata validation passed — now get initial lookaheads from LOCAL cursors
    auto local_cursor_a = std::make_unique<ValidatedReplayCursor>(std::move(*cursor_a_ptr));
    auto local_cursor_b = std::make_unique<ValidatedReplayCursor>(std::move(*cursor_b_ptr));

    auto local_lookahead_a = local_cursor_a->next();
    auto local_lookahead_b = local_cursor_b->next();

    auto is_terminal = [](ReplayCursorCode c) {
        return c == ReplayCursorCode::IoError ||
               c == ReplayCursorCode::StreamChanged ||
               c == ReplayCursorCode::InternalInvariantViolation;
    };

    if (is_terminal(local_lookahead_a.code)) {
        // Aggregate cursor remains Uninitialized: nothing written to impl_
        result.code = map_child_code(local_lookahead_a.code);
        result.issues.push_back({ValidationSeverity::Error, "CHILD_A_INIT_ERROR",
                                 "Child cursor A failed during initial read", {}, {}});
        return result;
    }

    if (is_terminal(local_lookahead_b.code)) {
        // Aggregate cursor remains Uninitialized: nothing written to impl_
        result.code = map_child_code(local_lookahead_b.code);
        result.issues.push_back({ValidationSeverity::Error, "CHILD_B_INIT_ERROR",
                                 "Child cursor B failed during initial read", {}, {}});
        return result;
    }

    // Both lookaheads OK — commit everything to impl_
    impl_->cursor_a = std::move(local_cursor_a);
    impl_->cursor_b = std::move(local_cursor_b);
    impl_->meta_a = *meta_a_ptr;
    impl_->meta_b = *meta_b_ptr;
    impl_->has_meta_a = true;
    impl_->has_meta_b = true;
    impl_->lookahead_a = local_lookahead_a;
    impl_->lookahead_b = local_lookahead_b;

    impl_->a_alive = (impl_->lookahead_a.code == ReplayCursorCode::Ok);
    impl_->b_alive = (impl_->lookahead_b.code == ReplayCursorCode::Ok);
    impl_->a_end = (impl_->lookahead_a.code == ReplayCursorCode::End);
    impl_->b_end = (impl_->lookahead_b.code == ReplayCursorCode::End);

    impl_->state = AbReplayState::Ready;
    impl_->ever_initialized = true;
    impl_->last_issued = SourceSide::None;
    impl_->has_last_key = false;

    result.code = AbReplayCode::Ok;
    return result;
}

AbReplayResult ValidatedAbReplayCursor::next() {
    AbReplayResult result;

    if (impl_->state == AbReplayState::Uninitialized) {
        result.code = AbReplayCode::NotInitialized;
        result.side = SourceSide::None;
        return result;
    }
    if (impl_->state == AbReplayState::End) {
        result.code = AbReplayCode::End;
        result.side = SourceSide::None;
        return result;
    }
    if (impl_->state == AbReplayState::Failed) {
        result.code = impl_->terminal_code;
        result.side = SourceSide::None;
        return result;
    }

    // State is Ready — advance previously issued side
    if (impl_->last_issued == SourceSide::A) {
        impl_->lookahead_a = impl_->cursor_a->next();
        if (impl_->lookahead_a.code == ReplayCursorCode::Ok) {
            impl_->a_alive = true;
            impl_->a_end = false;
        } else if (impl_->lookahead_a.code == ReplayCursorCode::End) {
            impl_->a_alive = false;
            impl_->a_end = true;
        } else {
            impl_->a_alive = false;
            impl_->a_end = false;
            impl_->fail(map_child_code(impl_->lookahead_a.code), "CHILD_A_ERROR",
                        "Child cursor A failed during advance");
            result.code = impl_->terminal_code;
            result.side = SourceSide::None;
            return result;
        }
    } else if (impl_->last_issued == SourceSide::B) {
        impl_->lookahead_b = impl_->cursor_b->next();
        if (impl_->lookahead_b.code == ReplayCursorCode::Ok) {
            impl_->b_alive = true;
            impl_->b_end = false;
        } else if (impl_->lookahead_b.code == ReplayCursorCode::End) {
            impl_->b_alive = false;
            impl_->b_end = true;
        } else {
            impl_->b_alive = false;
            impl_->b_end = false;
            impl_->fail(map_child_code(impl_->lookahead_b.code), "CHILD_B_ERROR",
                        "Child cursor B failed during advance");
            result.code = impl_->terminal_code;
            result.side = SourceSide::None;
            return result;
        }
    }

    // Pick winner
    if (!impl_->a_alive && !impl_->b_alive) {
        if (impl_->a_end && impl_->b_end) {
            impl_->state = AbReplayState::End;
            result.code = AbReplayCode::End;
            result.side = SourceSide::None;
            return result;
        }
        impl_->fail(AbReplayCode::InternalInvariantViolation, "INVARIANT_VIOLATION",
                    "Both sides not alive and not both end");
        result.code = impl_->terminal_code;
        result.side = SourceSide::None;
        return result;
    }

    SourceSide winner;
    if (impl_->a_alive && !impl_->b_alive) {
        winner = SourceSide::A;
    } else if (!impl_->a_alive && impl_->b_alive) {
        winner = SourceSide::B;
    } else {
        auto a_key = make_merge_key(impl_->lookahead_a.record, SourceSide::A);
        auto b_key = make_merge_key(impl_->lookahead_b.record, SourceSide::B);
        winner = (a_key <= b_key) ? SourceSide::A : SourceSide::B;
    }

    const auto& winner_record = (winner == SourceSide::A)
                                    ? impl_->lookahead_a.record
                                    : impl_->lookahead_b.record;

    // Strict merge-key regression guard
    auto winner_key = make_merge_key(winner_record, winner);
    if (impl_->has_last_key && winner_key <= impl_->last_key) {
        impl_->fail(AbReplayCode::ClockRegression, "CLOCK_REGRESSION",
                    "Merge key regression detected");
        result.code = AbReplayCode::ClockRegression;
        result.side = SourceSide::None;
        return result;
    }

    impl_->last_key = winner_key;
    impl_->has_last_key = true;
    impl_->last_issued = winner;

    result.code = AbReplayCode::Ok;
    result.side = winner;
    result.record = winner_record;
    return result;
}

}  // namespace moex_raw
