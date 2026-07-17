#pragma once
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace moex_raw {

// --- Enums (exact v1 values) ---

enum class ClockDomain : std::uint8_t {
    Synthetic = 1,
    SystemMonotonicReceive = 2,
    HardwareReceive = 3
};

enum class Transport : std::uint8_t {
    Synthetic = 0,
    Udp = 1,
    Tcp = 2
};

enum class SourceSide : std::uint8_t {
    None = 0,
    A = 1,
    B = 2
};

// --- Segment constants ---

static constexpr std::uint8_t kMagicRaw[8] = {'M','X','R','A','W','V','1','\0'};
static constexpr std::uint8_t kMagicRec[4] = {'R','E','C','1'};
static constexpr std::uint8_t kMagicEnd[8] = {'M','X','E','N','D','V','1','\0'};
static constexpr std::uint8_t kMagicReplay[10] = {'M','X','R','E','P','L','A','Y','1','\0'};
static constexpr std::uint16_t kFormatVersion = 1;
static constexpr std::uint16_t kRecordHeaderSize = 44;
static constexpr std::uint32_t kFooterSize = 92;
static constexpr std::uint32_t kMaxPayloadSize = 1024 * 1024;  // 1 MiB
static constexpr std::uint64_t kMaxSegmentBytes = 64ULL * 1024 * 1024 * 1024;  // 64 GiB
static constexpr std::size_t kMaxHeaderSize = 4096;

// Record flag bit 0 = utc_timestamp_valid
static constexpr std::uint16_t kRecordFlagUtcValid = 0x0001;

// --- Value types ---

struct RawSessionMetadata {
    std::uint8_t session_id[16]{};
    std::string feed_group;
    std::string endpoint_role;
    std::string source_label;
};

struct RawSourceMetadata {
    ClockDomain clock_domain = ClockDomain::Synthetic;
    Transport transport = Transport::Synthetic;
    SourceSide source_side = SourceSide::None;
    std::uint64_t source_id = 0;
    std::uint64_t channel_id = 0;
    std::uint8_t configuration_sha256[32]{};
    std::uint8_t templates_sha256[32]{};
    std::uint8_t endpoint_fingerprint_sha256[32]{};
};

struct RawSegmentMetadata {
    RawSessionMetadata session;
    RawSourceMetadata source;
    std::uint64_t segment_index = 0;
    std::uint64_t start_capture_index = 0;
    std::uint64_t created_utc_ns = 0;
};

struct RawPacketRecord {
    std::uint16_t record_flags = 0;
    std::uint64_t capture_index = 0;
    std::uint64_t capture_utc_ns = 0;
    std::uint64_t capture_monotonic_ns = 0;
    std::vector<std::uint8_t> payload;
};

struct RawSegmentRotationPolicy {
    std::uint64_t max_records_per_segment = 0;
    std::uint64_t max_segment_bytes = 0;
};

struct RawReplaySummary {
    std::uint64_t record_count = 0;
    std::uint64_t total_payload_bytes = 0;
    std::uint64_t first_capture_index = 0;
    std::uint64_t last_capture_index = 0;
    std::string replay_sha256;
};

enum class ValidationSeverity {
    Warning,
    Error
};

enum class SegmentStatus {
    ValidFinalized,
    Partial,
    Truncated,
    Corrupt,
    Unsupported,
    IoError
};

struct RawValidationIssue {
    ValidationSeverity severity;
    std::string code;
    std::string message;
    std::string source;  // stream key or file path that produced this issue
    std::string path;    // concrete file path, if applicable
};

struct RawStreamSummary {
    std::string session_id_hex;
    std::uint64_t source_id = 0;
    std::uint64_t channel_id = 0;
    std::string feed_group;
    std::string endpoint_role;
    std::string source_label;
    std::string clock_domain;
    std::string transport;
    std::string source_side;
    std::string configuration_sha256;
    std::string templates_sha256;
    std::string endpoint_fingerprint_sha256;
    std::string stream_key;
    std::vector<std::uint64_t> segment_indexes;
    std::vector<std::uint64_t> segment_sizes;
    std::vector<std::string> segment_content_sha256;  // per-segment content SHA-256
    std::vector<std::string> segment_file_sha256;      // per-segment file SHA-256
    std::string content_sha256;  // aggregate (empty for multi-segment)
    std::string file_sha256;     // aggregate (empty for multi-segment)
    std::uint64_t record_count = 0;
    std::uint64_t total_payload_bytes = 0;
    std::uint64_t first_capture_index = 0;
    std::uint64_t last_capture_index = 0;
    std::uint64_t first_capture_utc_ns = 0;
    std::uint64_t last_capture_utc_ns = 0;
    std::string status;
};

struct RawSegmentReport {
    std::string schema_version = "1.0";
    std::string tool_version = "0.1.0";
    std::string format_version = "1";
    std::string operation;
    std::vector<std::string> input_paths;
    std::string session_id_hex;
    std::uint64_t source_id = 0;
    std::uint64_t channel_id = 0;
    std::string feed_group;
    std::string endpoint_role;
    std::string source_label;
    std::string clock_domain;
    std::string transport;
    std::string source_side;
    std::string configuration_sha256;
    std::string templates_sha256;
    std::string endpoint_fingerprint_sha256;
    std::string stream_key;
    std::vector<std::uint64_t> segment_indexes;
    std::vector<std::uint64_t> segment_sizes;
    std::string content_sha256;
    std::string file_sha256;
    std::uint64_t record_count = 0;
    std::uint64_t total_payload_bytes = 0;
    std::uint64_t first_capture_index = 0;
    std::uint64_t last_capture_index = 0;
    std::uint64_t first_capture_utc_ns = 0;
    std::uint64_t last_capture_utc_ns = 0;
    std::vector<RawValidationIssue> issues;
    std::string replay_sha256;
    std::string overall_status;  // valid, warning, invalid
    std::vector<RawStreamSummary> stream_sets;
};

// --- Serialization helpers ---

void serialize_header(std::vector<std::uint8_t>& buf, const RawSegmentMetadata& meta);
bool deserialize_header(const std::uint8_t* data, std::size_t len, RawSegmentMetadata& meta,
                        std::size_t& header_size, std::vector<RawValidationIssue>& issues);

void serialize_record(std::vector<std::uint8_t>& buf, const RawPacketRecord& rec);

struct RawPacketRecordView {
    std::uint16_t record_flags = 0;
    std::uint64_t capture_index = 0;
    std::uint64_t capture_utc_ns = 0;
    std::uint64_t capture_monotonic_ns = 0;
    std::span<const std::uint8_t> payload;
};

bool deserialize_record_view(const std::uint8_t* data, std::size_t available,
                             RawPacketRecordView& out, std::size_t& record_total_size,
                             std::vector<RawValidationIssue>& issues);

bool deserialize_record_header(const std::uint8_t* data, std::size_t available,
                               RawPacketRecord& rec, std::size_t& record_total_size,
                               std::vector<RawValidationIssue>& issues);

// --- Replay cursor public types ---

enum class ReplayCursorState { Uninitialized, Ready, End, Failed };

enum class ReplayCursorCode {
    Ok,
    End,
    NotInitialized,
    AlreadyInitialized,
    ValidationFailed,
    IoError,
    StreamChanged,
    InternalInvariantViolation
};

struct ReplayCursorInitResult {
    ReplayCursorCode code = ReplayCursorCode::Ok;
    SegmentStatus segment_status = SegmentStatus::ValidFinalized;
    std::vector<RawValidationIssue> issues;
};

struct ReplayCursorResult {
    ReplayCursorCode code = ReplayCursorCode::Ok;
    RawPacketRecordView record;
};

struct RawFooter {
    std::uint64_t record_count = 0;
    std::uint64_t first_capture_index = 0;
    std::uint64_t last_capture_index = 0;
    std::uint64_t total_payload_bytes = 0;
    std::uint64_t data_bytes_before_footer = 0;
    std::uint8_t content_sha256[32]{};
};

void serialize_footer(std::vector<std::uint8_t>& buf, const RawFooter& footer);
bool deserialize_footer(const std::uint8_t* data, std::size_t available,
                        RawFooter& footer, std::vector<RawValidationIssue>& issues);

// Session ID hex helpers
std::string session_id_hex(const std::uint8_t id[16]);
bool parse_session_id_hex(const std::string& hex, std::uint8_t id[16]);

// Source metadata hex helpers
std::string source_id_hex(std::uint64_t id);
std::string channel_id_hex(std::uint64_t id);
std::string segment_index_hex(std::uint64_t idx);

// Canonical filename
std::string canonical_filename(const std::uint8_t session_id[16], std::uint64_t source_id,
                               std::uint64_t channel_id, std::uint64_t segment_index);

// Parse segment index from filename
bool parse_segment_index_from_filename(const std::string& filename, std::uint64_t& segment_index);

// Parsed canonical filename components
struct ParsedFilename {
    std::uint8_t session_id[16]{};
    std::uint64_t source_id = 0;
    std::uint64_t channel_id = 0;
    std::uint64_t segment_index = 0;
};

// Parse full canonical filename. Returns false on any malformed input.
bool parse_canonical_filename(const std::string& filename, ParsedFilename& parsed);

// SHA-256 hex from 32 bytes
std::string sha256_bytes_to_hex(const std::uint8_t hash[32]);

}  // namespace moex_raw
