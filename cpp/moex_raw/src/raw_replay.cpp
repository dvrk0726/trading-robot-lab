#include "moex_raw/raw_replay.hpp"
#include "moex_raw/raw_segment.hpp"
#include "moex_raw/raw_types.hpp"
#include "moex_raw/endian.hpp"
#include "moex_raw/sha256.hpp"
#include "moex_raw/file_position.hpp"
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <algorithm>
#include <vector>

namespace moex_raw {

static void add_replay_issue(std::vector<RawValidationIssue>& issues,
                             ValidationSeverity sev, const std::string& code,
                             const std::string& msg) {
    issues.push_back({sev, code, msg});
}

ReplayResult replay_stream(const std::vector<std::string>& sorted_paths,
                           const RawSegmentMetadata& /*meta*/,
                           ReplayCallback callback) {
    ReplayResult result;

    if (sorted_paths.empty()) {
        add_replay_issue(result.issues, ValidationSeverity::Error,
                         "EMPTY_STREAM", "No segments to replay");
        result.status = ReplayStatus::ValidationFailed;
        return result;
    }

    // Validate the stream set first
    std::vector<RawSegmentMetadata> metas;
    std::vector<RawFooter> footers;
    auto status = validate_stream_set(sorted_paths, metas, footers, result.issues);
    if (status != SegmentStatus::ValidFinalized) {
        result.status = ReplayStatus::ValidationFailed;
        return result;
    }

    // Replay records in order — bounded streaming per segment
    std::uint64_t total_records = 0;
    std::uint64_t total_payload = 0;
    std::uint64_t first_index = 0;
    std::uint64_t last_index = 0;
    bool first = true;

    for (std::size_t si = 0; si < sorted_paths.size(); ++si) {
        std::FILE* f = std::fopen(sorted_paths[si].c_str(), "rb");
        if (!f) {
            result.status = ReplayStatus::IoError;
            return result;
        }

        // Get file size using std::filesystem (supports > 4 GiB on all platforms)
        std::error_code ec;
        auto file_size = static_cast<std::uint64_t>(std::filesystem::file_size(sorted_paths[si], ec));
        if (ec) {
            std::fclose(f);
            result.status = ReplayStatus::IoError;
            return result;
        }

        // Read header size (bounded: first 14 bytes)
        std::uint8_t preamble[14];
        if (std::fread(preamble, 1, 14, f) != 14) { std::fclose(f); result.status = ReplayStatus::IoError; return result; }
        std::size_t header_size = read_u32_le(preamble + 10);

        // Seek past header
        if (fseek64(f, static_cast<std::int64_t>(header_size), SEEK_SET) != 0) { std::fclose(f); result.status = ReplayStatus::IoError; return result; }

        std::uint64_t data_end = file_size - kFooterSize;
        std::uint64_t pos = header_size;

        // Read records one at a time (bounded streaming)
        while (pos < data_end) {
            // Read record header
            std::uint8_t rec_hdr[kRecordHeaderSize];
            if (std::fread(rec_hdr, 1, kRecordHeaderSize, f) != kRecordHeaderSize) {
                std::fclose(f);
                result.status = ReplayStatus::IoError;
                return result;
            }

            // Parse record size
            std::uint32_t record_size = read_u32_le(rec_hdr + 8);
            std::uint32_t payload_size = read_u32_le(rec_hdr + 36);

            if (record_size != kRecordHeaderSize + payload_size + 4) {
                std::fclose(f);
                result.status = ReplayStatus::ValidationFailed;
                add_replay_issue(result.issues, ValidationSeverity::Error,
                                 "WRONG_RECORD_SIZE", "record_size mismatch");
                return result;
            }

            // Read remaining record bytes
            std::size_t tail_bytes = record_size - kRecordHeaderSize;
            std::vector<std::uint8_t> rec_tail(tail_bytes);
            if (std::fread(rec_tail.data(), 1, tail_bytes, f) != tail_bytes) {
                std::fclose(f);
                result.status = ReplayStatus::IoError;
                return result;
            }

            // Build full record buffer for deserialization
            std::vector<std::uint8_t> full_rec(rec_hdr, rec_hdr + kRecordHeaderSize);
            full_rec.insert(full_rec.end(), rec_tail.begin(), rec_tail.end());

            RawPacketRecord rec;
            std::size_t record_total_size = 0;
            std::vector<RawValidationIssue> rec_issues;

            if (!deserialize_record_header(full_rec.data(), full_rec.size(),
                                           rec, record_total_size, rec_issues)) {
                std::fclose(f);
                result.status = ReplayStatus::ValidationFailed;
                result.issues.insert(result.issues.end(), rec_issues.begin(), rec_issues.end());
                return result;
            }

            // Call callback with bounded payload (rec.payload lifetime = callback scope)
            if (!callback(metas[si], rec)) {
                std::fclose(f);
                result.status = ReplayStatus::Aborted;
                result.summary.record_count = total_records;
                result.summary.total_payload_bytes = total_payload;
                if (!first) {
                    result.summary.first_capture_index = first_index;
                    result.summary.last_capture_index = last_index;
                }
                return result;
            }

            if (first) {
                first_index = rec.capture_index;
                first = false;
            }
            last_index = rec.capture_index;
            total_records++;
            total_payload += rec.payload.size();

            pos += record_size;
        }

        std::fclose(f);
    }

    result.summary.record_count = total_records;
    result.summary.total_payload_bytes = total_payload;
    result.summary.first_capture_index = first_index;
    result.summary.last_capture_index = last_index;

    result.status = ReplayStatus::Ok;
    return result;
}

ReplayResult replay_from_directory(const std::string& directory,
                                   std::uint64_t source_id,
                                   std::uint64_t channel_id,
                                   ReplayCallback callback) {
    ReplayResult result;
    std::vector<RawValidationIssue> discovery_issues;

    auto stream_sets = group_stream_sets(directory, discovery_issues);

    // Propagate discovery issues
    result.issues.insert(result.issues.end(), discovery_issues.begin(), discovery_issues.end());

    // Find matching stream sets by (source_id, channel_id)
    std::vector<StreamSetInfo*> matches;
    for (auto& ss : stream_sets) {
        if (ss.source_id == source_id && ss.channel_id == channel_id) {
            matches.push_back(&ss);
        }
    }

    if (matches.empty()) {
        if (stream_sets.empty()) {
            result.status = ReplayStatus::StreamNotFound;
            add_replay_issue(result.issues, ValidationSeverity::Error,
                             "NO_STREAM", "No valid stream sets found in directory");
        } else {
            result.status = ReplayStatus::StreamNotFound;
            add_replay_issue(result.issues, ValidationSeverity::Error,
                             "STREAM_NOT_FOUND",
                             "Stream with specified source_id/channel_id not found");
        }
        return result;
    }

    // Check for ambiguity: multiple sessions with same source_id/channel_id
    if (matches.size() > 1) {
        bool different_sessions = false;
        for (std::size_t i = 1; i < matches.size(); ++i) {
            if (std::memcmp(matches[i]->session_id, matches[0]->session_id, 16) != 0) {
                different_sessions = true;
                break;
            }
        }
        if (different_sessions) {
            result.status = ReplayStatus::AmbiguousStream;
            add_replay_issue(result.issues, ValidationSeverity::Error,
                             "AMBIGUOUS_STREAM",
                             "Multiple sessions with same source_id/channel_id; specify session_id");
            return result;
        }
    }

    StreamSetInfo* target = matches[0];

    // Build metadata from first segment
    std::vector<RawSegmentMetadata> metas;
    std::vector<RawFooter> footers;
    std::vector<RawValidationIssue> issues;

    auto status = validate_stream_set(target->segment_paths, metas, footers, issues);
    if (status != SegmentStatus::ValidFinalized || metas.empty()) {
        result.status = ReplayStatus::ValidationFailed;
        result.issues = std::move(issues);
        return result;
    }

    return replay_stream(target->segment_paths, metas[0], std::move(callback));
}

ReplayResult replay_from_stream_set(const StreamSetInfo& stream_set,
                                    ReplayCallback callback) {
    ReplayResult result;

    // Build metadata from first segment
    std::vector<RawSegmentMetadata> metas;
    std::vector<RawFooter> footers;
    std::vector<RawValidationIssue> issues;

    auto status = validate_stream_set(stream_set.segment_paths, metas, footers, issues);
    if (status != SegmentStatus::ValidFinalized || metas.empty()) {
        result.status = ReplayStatus::ValidationFailed;
        result.issues = std::move(issues);
        return result;
    }

    return replay_stream(stream_set.segment_paths, metas[0], std::move(callback));
}

std::string compute_replay_sha256(const RawSegmentMetadata& meta,
                                  const std::vector<RawPacketRecord>& records) {
    std::vector<std::uint8_t> buf;

    // MXREPLAY1 magic
    write_bytes(buf, kMagicReplay, 10);

    // session_id
    write_bytes(buf, meta.session.session_id, 16);

    // source_id, channel_id
    write_u64_le(buf, meta.source.source_id);
    write_u64_le(buf, meta.source.channel_id);

    // clock_domain, transport, source_side
    buf.push_back(static_cast<std::uint8_t>(meta.source.clock_domain));
    buf.push_back(static_cast<std::uint8_t>(meta.source.transport));
    buf.push_back(static_cast<std::uint8_t>(meta.source.source_side));

    // SHA-256 values
    write_bytes(buf, meta.source.configuration_sha256, 32);
    write_bytes(buf, meta.source.templates_sha256, 32);
    write_bytes(buf, meta.source.endpoint_fingerprint_sha256, 32);

    // Strings
    write_u16_le(buf, static_cast<std::uint16_t>(meta.session.feed_group.size()));
    write_bytes(buf, meta.session.feed_group.data(), meta.session.feed_group.size());
    write_u16_le(buf, static_cast<std::uint16_t>(meta.session.endpoint_role.size()));
    write_bytes(buf, meta.session.endpoint_role.data(), meta.session.endpoint_role.size());
    write_u16_le(buf, static_cast<std::uint16_t>(meta.session.source_label.size()));
    write_bytes(buf, meta.session.source_label.data(), meta.session.source_label.size());

    // Records
    for (const auto& rec : records) {
        write_u16_le(buf, rec.record_flags);
        write_u64_le(buf, rec.capture_index);
        write_u64_le(buf, rec.capture_utc_ns);
        write_u64_le(buf, rec.capture_monotonic_ns);
        write_u32_le(buf, static_cast<std::uint32_t>(rec.payload.size()));
        write_bytes(buf, rec.payload.data(), rec.payload.size());
    }

    return sha256_hex(buf.data(), buf.size());
}

}  // namespace moex_raw
