#pragma once
#include "moex_raw/raw_types.hpp"
#include "moex_raw/sha256.hpp"
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

// File handle abstraction for deterministic I/O injection.
struct IFileHandle {
    virtual ~IFileHandle() = default;
    // Read up to size bytes. Returns actual bytes read (0 on EOF/error).
    virtual std::size_t read(void* buf, std::size_t size) = 0;
    // Write size bytes. Returns actual bytes written.
    virtual std::size_t write(const void* buf, std::size_t size) = 0;
    // Seek to offset from origin. Returns false on error.
    virtual bool seek(std::int64_t offset, int origin) = 0;
    // Flush buffered data. Returns false on error.
    virtual bool flush() = 0;
    // Close the handle. Returns false on error.
    virtual bool close() = 0;
};

// Filesystem operations abstraction for testability.
struct IFileSystem {
    virtual ~IFileSystem() = default;
    virtual bool exists(const std::string& path) = 0;
    virtual bool rename(const std::string& from, const std::string& to) = 0;
    virtual bool remove(const std::string& path) = 0;
    // Returns file size; sets ok=false on error.
    virtual std::uint64_t file_size(const std::string& path, bool& ok) = 0;
    // Open file for reading. Returns nullptr on failure.
    virtual std::unique_ptr<IFileHandle> open_read(const std::string& path) = 0;
    // Open file for writing (creates/truncates). Returns nullptr on failure.
    virtual std::unique_ptr<IFileHandle> open_write(const std::string& path) = 0;
};

// Default file handle wrapping std::FILE*.
struct DefaultFileHandle : IFileHandle {
    std::FILE* f_ = nullptr;
    explicit DefaultFileHandle(std::FILE* f) : f_(f) {}
    ~DefaultFileHandle() override;
    std::size_t read(void* buf, std::size_t size) override;
    std::size_t write(const void* buf, std::size_t size) override;
    bool seek(std::int64_t offset, int origin) override;
    bool flush() override;
    bool close() override;
};

struct DefaultFileSystem : IFileSystem {
    bool exists(const std::string& path) override;
    bool rename(const std::string& from, const std::string& to) override;
    bool remove(const std::string& path) override;
    std::uint64_t file_size(const std::string& path, bool& ok) override;
    std::unique_ptr<IFileHandle> open_read(const std::string& path) override;
    std::unique_ptr<IFileHandle> open_write(const std::string& path) override;
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
    std::unique_ptr<IFileHandle> file_;
    std::uint64_t record_count_ = 0;
    std::uint64_t total_payload_bytes_ = 0;
    std::uint64_t current_segment_index_ = 0;
    std::uint64_t next_capture_index_ = 0;
    std::uint64_t current_file_bytes_ = 0;
    std::uint64_t last_monotonic_ns_ = 0;
    std::vector<std::uint8_t> content_buffer_;  // for SHA-256
    SHA256Ctx content_sha_ctx_;                  // incremental content SHA-256
    bool content_sha_initialized_ = false;
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
// first_monotonic_ns / last_monotonic_ns populated from actual record data.
// first_utc_ns / last_utc_ns populated from records with kRecordFlagUtcValid (0 if none).
SegmentStatus validate_segment(const std::string& path,
                               RawSegmentMetadata& meta,
                               RawFooter& footer,
                               std::vector<RawValidationIssue>& issues,
                               std::string& content_sha256_hex,
                               std::string& file_sha256_hex,
                               std::uint64_t* first_monotonic_ns = nullptr,
                               std::uint64_t* last_monotonic_ns = nullptr,
                               std::uint64_t* first_utc_ns = nullptr,
                               std::uint64_t* last_utc_ns = nullptr,
                               IFileSystem* fs = nullptr);

// Validate a stream set (multiple segments for one source).
// first/last_capture_utc_ns: across all segments from records with kRecordFlagUtcValid.
SegmentStatus validate_stream_set(const std::vector<std::string>& paths,
                                  std::vector<RawSegmentMetadata>& metas,
                                  std::vector<RawFooter>& footers,
                                  std::vector<RawValidationIssue>& issues,
                                  std::uint64_t* first_capture_utc_ns = nullptr,
                                  std::uint64_t* last_capture_utc_ns = nullptr,
                                  IFileSystem* fs = nullptr);

// Group segments in a directory by (session_id, source_id, channel_id).
// Issues from unreadable, malformed, corrupt, unsupported, and partial candidates
// are reported in `issues`. No .mxraw or .mxraw.partial candidate is silently skipped.
std::vector<StreamSetInfo> group_stream_sets(const std::string& directory,
                                              std::vector<RawValidationIssue>& issues,
                                              IFileSystem* fs = nullptr);

}  // namespace moex_raw
