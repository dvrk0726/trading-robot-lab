#pragma once
#include "moex_raw/raw_types.hpp"
#include <cstdint>
#include <string>
#include <vector>
#include <functional>

namespace moex_raw {

struct StreamSetInfo;

// Callback receives immutable metadata and bounded payload view.
// Return false to stop replay.
using ReplayCallback = std::function<bool(const RawSegmentMetadata& meta,
                                          const RawPacketRecord& rec)>;

enum class ReplayStatus {
    Ok,
    Aborted,       // callback returned false
    ValidationFailed,
    AmbiguousStream,
    StreamNotFound,
    IoError
};

struct ReplayResult {
    ReplayStatus status = ReplayStatus::Ok;
    RawReplaySummary summary;
    std::vector<RawValidationIssue> issues;
};

// Replay a single stream set. Paths must be sorted by segment index.
// metadata must contain the stream's session/source metadata.
ReplayResult replay_stream(const std::vector<std::string>& sorted_paths,
                           const RawSegmentMetadata& meta,
                           ReplayCallback callback);

// Replay from a directory, selecting a specific (source_id, channel_id).
ReplayResult replay_from_directory(const std::string& directory,
                                   std::uint64_t source_id,
                                   std::uint64_t channel_id,
                                   ReplayCallback callback);

// Replay from a fully resolved StreamSetInfo (no ambiguity possible).
ReplayResult replay_from_stream_set(const StreamSetInfo& stream_set,
                                    ReplayCallback callback);

// Compute MXREPLAY1 digest from summary and metadata.
std::string compute_replay_sha256(const RawSegmentMetadata& meta,
                                  const std::vector<RawPacketRecord>& records);

}  // namespace moex_raw
