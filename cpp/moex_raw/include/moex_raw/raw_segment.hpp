#pragma once
#include "moex_raw/raw_types.hpp"
#include <cstdint>
#include <string>
#include <vector>
#include <memory>

namespace moex_raw {

enum class WriterState {
    Created,
    Open,
    Finalized,
    Failed
};

// Filesystem operations abstraction for testability.
struct IFileSystem {
    virtual ~IFileSystem() = default;
    virtual bool exists(const std::string& path) = 0;
    virtual bool rename(const std::string& from, const std::string& to) = 0;
    virtual bool remove(const std::string& path) = 0;
};

struct DefaultFileSystem : IFileSystem {
    bool exists(const std::string& path) override;
    bool rename(const std::string& from, const std::string& to) override;
    bool remove(const std::string& path) override;
};

class RawSegmentWriter {
public:
    // metadata: all fields must be explicitly set (no random IDs, no wall clock).
    // output_dir: directory for the segment file.
    // policy: rotation limits (0 means no limit for that axis).
    // fs: optional filesystem abstraction (default uses real filesystem).
    RawSegmentWriter(RawSegmentMetadata metadata,
                     std::string output_dir,
                     RawSegmentRotationPolicy policy,
                     IFileSystem* fs = nullptr);

    ~RawSegmentWriter();

    // Open the writer — creates the .partial file.
    // Returns empty string on success, error message on failure.
    std::string open();

    // Append a record. capture_index must be exactly one more than the previous.
    // Returns empty string on success, error message on failure.
    // If rotation triggers, the current segment is finalized and a new one starts.
    std::string append(const RawPacketRecord& rec);

    // Finalize the current segment — writes footer, flushes, closes, renames.
    // Returns empty string on success, error message on failure.
    std::string finalize();

    WriterState state() const { return state_; }
    std::uint64_t record_count() const { return record_count_; }
    std::uint64_t total_payload_bytes() const { return total_payload_bytes_; }
    std::uint64_t current_segment_index() const { return current_segment_index_; }
    std::uint64_t next_capture_index() const { return next_capture_index_; }

    // Get list of finalized segment paths.
    const std::vector<std::string>& finalized_paths() const { return finalized_paths_; }

    // Get the current partial path.
    std::string current_partial_path() const;

    // Get the current final path.
    std::string current_final_path() const;

private:
    std::string write_header();
    std::string write_record(const RawPacketRecord& rec);
    std::string finalize_current();
    std::string start_new_segment();
    std::size_t estimate_segment_bytes() const;
    std::string compute_content_sha256() const;
    bool should_rotate(const RawPacketRecord& rec) const;

    RawSegmentMetadata metadata_;
    std::string output_dir_;
    RawSegmentRotationPolicy policy_;
    IFileSystem* fs_;
    DefaultFileSystem default_fs_;
    WriterState state_ = WriterState::Created;
    std::FILE* file_ = nullptr;
    std::uint64_t record_count_ = 0;
    std::uint64_t total_payload_bytes_ = 0;
    std::uint64_t current_segment_index_ = 0;
    std::uint64_t next_capture_index_ = 0;
    std::uint64_t current_file_bytes_ = 0;
    std::vector<std::uint8_t> content_buffer_;  // for SHA-256
    std::vector<std::string> finalized_paths_;
};

// --- Reader/Validator ---

enum class StreamSetKey {
    SessionSourceChannel
};

struct StreamSetInfo {
    std::uint8_t session_id[16]{};
    std::uint64_t source_id = 0;
    std::uint64_t channel_id = 0;
    std::vector<std::string> segment_paths;
    std::vector<std::uint64_t> segment_indexes;
};

// Validate a single segment file. Returns status and issues.
SegmentStatus validate_segment(const std::string& path,
                               RawSegmentMetadata& meta,
                               RawFooter& footer,
                               std::vector<RawValidationIssue>& issues,
                               std::string& content_sha256_hex,
                               std::string& file_sha256_hex);

// Validate a stream set (multiple segments for one source).
SegmentStatus validate_stream_set(const std::vector<std::string>& paths,
                                  std::vector<RawSegmentMetadata>& metas,
                                  std::vector<RawFooter>& footers,
                                  std::vector<RawValidationIssue>& issues);

// Group segments in a directory by (session_id, source_id, channel_id).
std::vector<StreamSetInfo> group_stream_sets(const std::string& directory);

}  // namespace moex_raw
