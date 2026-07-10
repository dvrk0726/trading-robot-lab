#include "moex_raw/raw_types.hpp"
#include "moex_raw/raw_segment.hpp"
#include "moex_raw/raw_replay.hpp"
#include "moex_raw/raw_report.hpp"
#include "moex_raw/sha256.hpp"
#include "moex_raw/crc32c.hpp"
#include "moex_raw/endian.hpp"
#include "moex_raw/strings.hpp"
#include <iostream>
#include <fstream>
#include <cstring>
#include <filesystem>
#include <vector>
#include <string>
#include <cstdlib>
#include <cerrno>

using namespace moex_raw;

static void print_help() {
    std::cout << "moex-raw — MXRaw segment tool\n\n"
              << "Usage:\n"
              << "  moex-raw synth   [options]   Create deterministic synthetic segments\n"
              << "  moex-raw inspect [options]   Validate segment(s) and print report\n"
              << "  moex-raw replay  [options]   Replay a stream set and compute digest\n"
              << "\nRun 'moex-raw <command> --help' for command-specific help.\n";
}

static void print_synth_help() {
    std::cout << "moex-raw synth — Create synthetic segments\n\n"
              << "Options:\n"
              << "  --out <dir>            Output directory (required)\n"
              << "  --session <hex32>      Session ID (32 hex chars)\n"
              << "  --source <hex16>       Source ID (16 hex chars)\n"
              << "  --channel <hex16>      Channel ID (16 hex chars)\n"
              << "  --seed <uint>          Payload seed (default: 0)\n"
              << "  --records <uint>       Records per segment (default: 10)\n"
              << "  --segments <uint>      Number of segments (default: 1)\n"
              << "  --payload-size <uint>  Payload size in bytes (default: 64)\n"
              << "  --max-records <uint>   Max records per segment for rotation\n"
              << "  --max-bytes <uint>     Max segment bytes for rotation\n"
              << "  --feed-group <str>     Feed group name\n"
              << "  --endpoint-role <str>  Endpoint role\n"
              << "  --help                 Show this help\n";
}

static void print_inspect_help() {
    std::cout << "moex-raw inspect — Validate segment(s)\n\n"
              << "Options:\n"
              << "  --input <path>         Segment file or directory (required)\n"
              << "  --json-out <path>      Write JSON report to file\n"
              << "  --strict               Exit non-zero on warnings/errors\n"
              << "  --help                 Show this help\n";
}

static void print_replay_help() {
    std::cout << "moex-raw replay — Replay a stream set\n\n"
              << "Options:\n"
              << "  --input <dir>          Directory with segments (required)\n"
              << "  --session <hex32>      Session ID to select (32 hex chars)\n"
              << "  --source <hex16>       Source ID to select (16 hex chars)\n"
              << "  --channel <hex16>      Channel ID to select (16 hex chars)\n"
              << "  --json-out <path>      Write JSON report to file\n"
              << "  --help                 Show this help\n";
}

// Strict hex parsing — returns false on any malformed input
static bool parse_hex_u64_strict(const std::string& s, std::uint64_t& out) {
    if (s.empty()) return false;
    // Reject negative signs, whitespace, leading +
    if (s[0] == '-' || s[0] == '+' || s[0] == ' ' || s[0] == '\t') return false;
    if (s.find(' ') != std::string::npos || s.find('\t') != std::string::npos) return false;
    char* end = nullptr;
    errno = 0;
    out = std::strtoull(s.c_str(), &end, 16);
    if (end != s.c_str() + s.size()) return false;
    if (errno == ERANGE) return false;
    return true;
}

// Strict decimal parsing — returns false on any malformed input
static bool parse_u64_strict(const std::string& s, std::uint64_t& out) {
    if (s.empty()) return false;
    // Reject negative signs, whitespace, leading +
    if (s[0] == '-' || s[0] == '+' || s[0] == ' ' || s[0] == '\t') return false;
    if (s.find(' ') != std::string::npos || s.find('\t') != std::string::npos) return false;
    char* end = nullptr;
    errno = 0;
    out = std::strtoull(s.c_str(), &end, 10);
    if (end != s.c_str() + s.size()) return false;
    if (errno == ERANGE) return false;
    return true;
}

static std::vector<std::uint8_t> make_synthetic_payload(std::uint64_t seed, std::size_t size) {
    std::vector<std::uint8_t> payload(size);
    for (std::size_t i = 0; i < size; ++i) {
        payload[i] = static_cast<std::uint8_t>((seed + i) & 0xFF);
    }
    return payload;
}

static std::string clock_domain_name(ClockDomain cd) {
    switch (cd) {
        case ClockDomain::Synthetic: return "synthetic";
        case ClockDomain::SystemMonotonicReceive: return "system_monotonic_receive";
        case ClockDomain::HardwareReceive: return "hardware_receive";
        default: return "unknown";
    }
}

static std::string transport_name(Transport t) {
    switch (t) {
        case Transport::Synthetic: return "synthetic";
        case Transport::Udp: return "udp";
        case Transport::Tcp: return "tcp";
        default: return "unknown";
    }
}

static std::string source_side_name(SourceSide s) {
    switch (s) {
        case SourceSide::None: return "none";
        case SourceSide::A: return "A";
        case SourceSide::B: return "B";
        default: return "unknown";
    }
}

static RawStreamSummary build_stream_summary(const StreamSetInfo& ss,
                                              const std::vector<RawSegmentMetadata>& metas,
                                              const std::vector<RawFooter>& footers,
                                              const std::vector<std::string>& paths,
                                              const std::string& status,
                                              const std::vector<std::string>& content_hashes = {},
                                              const std::vector<std::string>& file_hashes = {},
                                              std::uint64_t first_utc_ns = 0,
                                              std::uint64_t last_utc_ns = 0) {
    RawStreamSummary summary;
    summary.session_id_hex = session_id_hex(ss.session_id);
    summary.source_id = ss.source_id;
    summary.channel_id = ss.channel_id;
    summary.stream_key = summary.session_id_hex + "_src" + source_id_hex(ss.source_id) +
                         "_ch" + channel_id_hex(ss.channel_id);
    summary.status = status;

    if (!metas.empty()) {
        summary.feed_group = metas[0].session.feed_group;
        summary.endpoint_role = metas[0].session.endpoint_role;
        summary.source_label = metas[0].session.source_label;
        summary.clock_domain = clock_domain_name(metas[0].source.clock_domain);
        summary.transport = transport_name(metas[0].source.transport);
        summary.source_side = source_side_name(metas[0].source.source_side);
        summary.configuration_sha256 = sha256_bytes_to_hex(metas[0].source.configuration_sha256);
        summary.templates_sha256 = sha256_bytes_to_hex(metas[0].source.templates_sha256);
        summary.endpoint_fingerprint_sha256 = sha256_bytes_to_hex(metas[0].source.endpoint_fingerprint_sha256);
    }

    for (std::size_t i = 0; i < metas.size(); ++i) {
        summary.segment_indexes.push_back(metas[i].segment_index);
        std::error_code ec;
        auto sz = std::filesystem::file_size(paths[i], ec);
        summary.segment_sizes.push_back(ec ? 0 : sz);
    }

    // Per-segment hashes
    summary.segment_content_sha256 = content_hashes;
    summary.segment_file_sha256 = file_hashes;

    // One-segment aggregate hashes (Blocker 5)
    if (content_hashes.size() == 1) {
        summary.content_sha256 = content_hashes[0];
        summary.file_sha256 = file_hashes[0];
    }

    for (const auto& f : footers) {
        summary.record_count += f.record_count;
        summary.total_payload_bytes += f.total_payload_bytes;
    }
    if (!footers.empty()) {
        summary.first_capture_index = footers.front().first_capture_index;
        summary.last_capture_index = footers.back().last_capture_index;
    }

    // UTC bounds from actual record data (Blocker 3)
    summary.first_capture_utc_ns = first_utc_ns;
    summary.last_capture_utc_ns = last_utc_ns;

    return summary;
}

static int cmd_synth(int argc, char* argv[]) {
    std::string out_dir;
    std::string session_hex = "0123456789abcdef0123456789abcdef";
    std::uint64_t source_id = 1;
    std::uint64_t channel_id = 1;
    std::uint64_t seed = 0;
    std::uint64_t records_per_seg = 10;
    std::uint64_t num_segments = 1;
    std::uint64_t payload_size = 64;
    std::uint64_t max_records = 0;
    std::uint64_t max_bytes = 0;
    std::string feed_group = "ORDERS-LOG";
    std::string endpoint_role = "Incremental";

    for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--help") { print_synth_help(); return 0; }
        if (arg == "--out" && i + 1 < argc) { out_dir = argv[++i]; }
        else if (arg == "--session" && i + 1 < argc) { session_hex = argv[++i]; }
        else if (arg == "--source" && i + 1 < argc) {
            if (!parse_hex_u64_strict(argv[++i], source_id)) {
                std::cerr << "Error: invalid --source hex value\n"; return 1;
            }
        }
        else if (arg == "--channel" && i + 1 < argc) {
            if (!parse_hex_u64_strict(argv[++i], channel_id)) {
                std::cerr << "Error: invalid --channel hex value\n"; return 1;
            }
        }
        else if (arg == "--seed" && i + 1 < argc) {
            if (!parse_u64_strict(argv[++i], seed)) {
                std::cerr << "Error: invalid --seed value\n"; return 1;
            }
        }
        else if (arg == "--records" && i + 1 < argc) {
            if (!parse_u64_strict(argv[++i], records_per_seg) || records_per_seg == 0) {
                std::cerr << "Error: invalid --records value\n"; return 1;
            }
        }
        else if (arg == "--segments" && i + 1 < argc) {
            if (!parse_u64_strict(argv[++i], num_segments) || num_segments == 0) {
                std::cerr << "Error: invalid --segments value\n"; return 1;
            }
        }
        else if (arg == "--payload-size" && i + 1 < argc) {
            if (!parse_u64_strict(argv[++i], payload_size)) {
                std::cerr << "Error: invalid --payload-size value\n"; return 1;
            }
        }
        else if (arg == "--max-records" && i + 1 < argc) {
            if (!parse_u64_strict(argv[++i], max_records)) {
                std::cerr << "Error: invalid --max-records value\n"; return 1;
            }
        }
        else if (arg == "--max-bytes" && i + 1 < argc) {
            if (!parse_u64_strict(argv[++i], max_bytes)) {
                std::cerr << "Error: invalid --max-bytes value\n"; return 1;
            }
        }
        else if (arg == "--feed-group" && i + 1 < argc) { feed_group = argv[++i]; }
        else if (arg == "--endpoint-role" && i + 1 < argc) { endpoint_role = argv[++i]; }
        else {
            std::cerr << "Unknown option: " << arg << "\n";
            return 1;
        }
    }

    // Validate all inputs before creating any files
    if (out_dir.empty()) {
        std::cerr << "Error: --out is required\n";
        return 1;
    }

    if (session_hex.size() != 32) {
        std::cerr << "Error: --session must be 32 hex chars\n";
        return 1;
    }

    if (source_id == 0) {
        std::cerr << "Error: --source must be non-zero\n";
        return 1;
    }

    if (channel_id == 0) {
        std::cerr << "Error: --channel must be non-zero\n";
        return 1;
    }

    if (payload_size > kMaxPayloadSize) {
        std::cerr << "Error: payload size exceeds 1 MiB\n";
        return 1;
    }

    if (feed_group.empty()) {
        std::cerr << "Error: --feed-group must be non-empty\n";
        return 1;
    }

    if (endpoint_role.empty()) {
        std::cerr << "Error: --endpoint-role must be non-empty\n";
        return 1;
    }

    // Validate feed_group and endpoint_role are valid UTF-8
    if (!validate_utf8_string(feed_group)) {
        std::cerr << "Error: --feed-group is not valid UTF-8\n";
        return 1;
    }
    if (!validate_utf8_string(endpoint_role)) {
        std::cerr << "Error: --endpoint-role is not valid UTF-8\n";
        return 1;
    }

    // Build metadata
    RawSegmentMetadata meta;
    if (!parse_session_id_hex(session_hex, meta.session.session_id)) {
        std::cerr << "Error: invalid session ID hex\n";
        return 1;
    }
    meta.session.feed_group = feed_group;
    meta.session.endpoint_role = endpoint_role;
    meta.session.source_label = "synthetic";
    meta.source.clock_domain = ClockDomain::Synthetic;
    meta.source.transport = Transport::Synthetic;
    meta.source.source_side = SourceSide::None;
    meta.source.source_id = source_id;
    meta.source.channel_id = channel_id;

    // Use deterministic SHA-256 hashes
    std::uint8_t config_hash[32], templates_hash[32], fingerprint_hash[32];
    sha256("synthetic_config", 16, config_hash);
    sha256("synthetic_templates", 18, templates_hash);
    sha256("synthetic_fingerprint", 21, fingerprint_hash);
    std::memcpy(meta.source.configuration_sha256, config_hash, 32);
    std::memcpy(meta.source.templates_sha256, templates_hash, 32);
    std::memcpy(meta.source.endpoint_fingerprint_sha256, fingerprint_hash, 32);

    meta.created_utc_ns = 1700000000000000000ULL;  // deterministic
    meta.segment_index = 0;
    meta.start_capture_index = 0;

    // Set up rotation policy
    // If --segments > 1 and no explicit rotation, use records_per_seg as rotation limit
    RawSegmentRotationPolicy policy;
    policy.max_records_per_segment = max_records;
    policy.max_segment_bytes = max_bytes;

    if (num_segments > 1 && max_records == 0 && max_bytes == 0) {
        policy.max_records_per_segment = records_per_seg;
    }

    RawSegmentWriter writer(meta, out_dir, policy);

    auto err = writer.open();
    if (!err.empty()) {
        std::cerr << "Error opening writer: " << err << "\n";
        return 1;
    }

    // Checked arithmetic for total records
    std::uint64_t total_records;
    if (!checked_mul_u64(records_per_seg, num_segments, total_records)) {
        std::cerr << "Error: records * segments overflow\n";
        return 1;
    }

    for (std::uint64_t i = 0; i < total_records; ++i) {
        RawPacketRecord packet;
        packet.record_flags = kRecordFlagUtcValid;
        packet.capture_index = i;

        // Checked arithmetic for synthetic timestamps
        std::uint64_t ts_offset;
        if (!checked_mul_u64(i, 1000000, ts_offset)) {
            std::cerr << "Error: timestamp overflow\n";
            return 1;
        }
        std::uint64_t utc_ns;
        if (!checked_add_u64(meta.created_utc_ns, ts_offset, utc_ns)) {
            std::cerr << "Error: timestamp overflow\n";
            return 1;
        }
        packet.capture_utc_ns = utc_ns;
        packet.capture_monotonic_ns = ts_offset;

        packet.payload = make_synthetic_payload(seed + i, static_cast<std::size_t>(payload_size));

        err = writer.append(packet);
        if (!err.empty()) {
            std::cerr << "Error appending record: " << err << "\n";
            return 1;
        }
    }

    // Finalize last segment if still open
    if (writer.state() == WriterState::Open) {
        err = writer.finalize();
        if (!err.empty()) {
            std::cerr << "Error finalizing last segment: " << err << "\n";
            return 1;
        }
    }

    std::cout << "Created " << writer.finalized_paths().size() << " segment(s) in " << out_dir << "\n";
    for (const auto& p : writer.finalized_paths()) {
        std::cout << "  " << p << "\n";
    }

    return 0;
}

static int cmd_inspect(int argc, char* argv[]) {
    std::string input_path;
    std::string json_out;
    bool strict = false;

    for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--help") { print_inspect_help(); return 0; }
        if (arg == "--input" && i + 1 < argc) { input_path = argv[++i]; }
        else if (arg == "--json-out" && i + 1 < argc) { json_out = argv[++i]; }
        else if (arg == "--strict") { strict = true; }
        else {
            std::cerr << "Unknown option: " << arg << "\n";
            return 1;
        }
    }

    if (input_path.empty()) {
        std::cerr << "Error: --input is required\n";
        return 1;
    }

    RawSegmentReport report;
    report.operation = "inspect";
    report.format_version = "1";

    bool has_errors = false;
    bool has_warnings = false;
    bool has_partial = false;

    if (std::filesystem::is_directory(input_path)) {
        std::vector<RawValidationIssue> discovery_issues;
        auto stream_sets = group_stream_sets(input_path, discovery_issues);
        report.input_paths.push_back(input_path);

        // Include discovery issues (unreadable, malformed, partial, etc.)
        report.issues.insert(report.issues.end(), discovery_issues.begin(), discovery_issues.end());
        for (const auto& issue : discovery_issues) {
            if (issue.severity == ValidationSeverity::Error) has_errors = true;
            if (issue.severity == ValidationSeverity::Warning) has_warnings = true;
            if (issue.code == "PARTIAL_FILE") has_partial = true;
        }

        if (stream_sets.empty()) {
            report.overall_status = "invalid";
            report.issues.push_back({ValidationSeverity::Error, "NO_STREAMS", "No valid streams found", {}, {}});
            has_errors = true;
        } else {
            // Sort stream_sets deterministically by (session_id, source_id, channel_id)
            std::sort(stream_sets.begin(), stream_sets.end(),
                      [](const StreamSetInfo& a, const StreamSetInfo& b) {
                          int cmp = std::memcmp(a.session_id, b.session_id, 16);
                          if (cmp != 0) return cmp < 0;
                          if (a.source_id != b.source_id) return a.source_id < b.source_id;
                          return a.channel_id < b.channel_id;
                      });

            // Process each stream set independently with separate summaries
            for (const auto& ss : stream_sets) {
                std::vector<RawSegmentMetadata> metas;
                std::vector<RawFooter> footers;
                std::vector<RawValidationIssue> issues;
                std::uint64_t first_utc = 0, last_utc = 0;

                auto status = validate_stream_set(ss.segment_paths, metas, footers, issues,
                                                  &first_utc, &last_utc);

                // Collect per-segment hashes
                std::vector<std::string> content_hashes, file_hashes;
                for (const auto& path : ss.segment_paths) {
                    RawSegmentMetadata tmp_meta;
                    RawFooter tmp_footer;
                    std::vector<RawValidationIssue> tmp_issues;
                    std::string ch, fh;
                    validate_segment(path, tmp_meta, tmp_footer, tmp_issues, ch, fh);
                    content_hashes.push_back(ch);
                    file_hashes.push_back(fh);
                }

                std::string stream_status;
                if (status == SegmentStatus::ValidFinalized) {
                    bool stream_has_issues = false;
                    for (const auto& issue : issues) {
                        if (issue.severity == ValidationSeverity::Error) stream_has_issues = true;
                    }
                    stream_status = stream_has_issues ? "invalid" : "valid";
                } else {
                    stream_status = "invalid";
                }

                auto summary = build_stream_summary(ss, metas, footers, ss.segment_paths,
                                                    stream_status, content_hashes, file_hashes,
                                                    first_utc, last_utc);
                report.stream_sets.push_back(summary);

                // Blocker 4: For multi-stream, DON'T fill singular top-level fields
                // Only fill for single stream
                if (stream_sets.size() == 1) {
                    report.session_id_hex = summary.session_id_hex;
                    report.feed_group = summary.feed_group;
                    report.endpoint_role = summary.endpoint_role;
                    report.source_label = summary.source_label;
                    report.stream_key = summary.stream_key;
                    report.source_id = summary.source_id;
                    report.channel_id = summary.channel_id;
                    report.configuration_sha256 = summary.configuration_sha256;
                    report.templates_sha256 = summary.templates_sha256;
                    report.endpoint_fingerprint_sha256 = summary.endpoint_fingerprint_sha256;
                    report.clock_domain = summary.clock_domain;
                    report.transport = summary.transport;
                    report.source_side = summary.source_side;
                    report.content_sha256 = summary.content_sha256;
                    report.file_sha256 = summary.file_sha256;
                    report.first_capture_utc_ns = summary.first_capture_utc_ns;
                    report.last_capture_utc_ns = summary.last_capture_utc_ns;

                    // Also set segment/count/index for single stream
                    for (std::size_t i = 0; i < metas.size(); ++i) {
                        report.segment_indexes.push_back(metas[i].segment_index);
                        std::error_code ec;
                        auto sz = std::filesystem::file_size(ss.segment_paths[i], ec);
                        report.segment_sizes.push_back(ec ? 0 : sz);
                    }
                    for (const auto& f : footers) {
                        report.record_count += f.record_count;
                        report.total_payload_bytes += f.total_payload_bytes;
                    }
                    if (!footers.empty()) {
                        report.first_capture_index = footers.front().first_capture_index;
                        report.last_capture_index = footers.back().last_capture_index;
                    }
                }

                // Add source info to issues — preserve per-file path (Blocker 5)
                auto stream_key = summary.stream_key;
                for (auto& issue : issues) {
                    issue.source = stream_key;
                    // Don't overwrite issue.path — it's already set by validate_segment
                }
                report.issues.insert(report.issues.end(), issues.begin(), issues.end());

                if (status != SegmentStatus::ValidFinalized) {
                    has_errors = true;
                }
                for (const auto& issue : issues) {
                    if (issue.severity == ValidationSeverity::Error) has_errors = true;
                    if (issue.severity == ValidationSeverity::Warning) has_warnings = true;
                }
            }

            if (has_errors) {
                report.overall_status = "invalid";
            } else if (has_warnings || has_partial) {
                report.overall_status = "warning";
            } else {
                report.overall_status = "valid";
            }
        }
    } else {
        // Single file
        report.input_paths.push_back(input_path);

        RawSegmentMetadata meta;
        RawFooter footer;
        std::vector<RawValidationIssue> issues;
        std::string content_hex, file_hex;
        std::uint64_t first_utc = 0, last_utc = 0;

        auto status = validate_segment(input_path, meta, footer, issues, content_hex, file_hex,
                                       nullptr, nullptr, &first_utc, &last_utc);

        report.session_id_hex = session_id_hex(meta.session.session_id);
        report.source_id = meta.source.source_id;
        report.channel_id = meta.source.channel_id;
        report.feed_group = meta.session.feed_group;
        report.endpoint_role = meta.session.endpoint_role;
        report.source_label = meta.session.source_label;
        report.clock_domain = clock_domain_name(meta.source.clock_domain);
        report.transport = transport_name(meta.source.transport);
        report.source_side = source_side_name(meta.source.source_side);
        report.configuration_sha256 = sha256_bytes_to_hex(meta.source.configuration_sha256);
        report.templates_sha256 = sha256_bytes_to_hex(meta.source.templates_sha256);
        report.endpoint_fingerprint_sha256 = sha256_bytes_to_hex(meta.source.endpoint_fingerprint_sha256);
        report.stream_key = report.session_id_hex + "_src" + source_id_hex(meta.source.source_id) +
                            "_ch" + channel_id_hex(meta.source.channel_id);
        report.segment_indexes.push_back(meta.segment_index);
        report.content_sha256 = content_hex;
        report.file_sha256 = file_hex;

        std::error_code ec;
        auto sz = std::filesystem::file_size(input_path, ec);
        report.segment_sizes.push_back(ec ? 0 : sz);

        report.record_count = footer.record_count;
        report.total_payload_bytes = footer.total_payload_bytes;
        report.first_capture_index = footer.first_capture_index;
        report.last_capture_index = footer.last_capture_index;
        report.first_capture_utc_ns = first_utc;
        report.last_capture_utc_ns = last_utc;

        // Add source/path to issues — path already set by validate_segment
        for (auto& issue : issues) {
            issue.source = report.stream_key;
        }
        report.issues = std::move(issues);

        for (const auto& issue : report.issues) {
            if (issue.severity == ValidationSeverity::Error) has_errors = true;
            if (issue.severity == ValidationSeverity::Warning) has_warnings = true;
        }

        if (status == SegmentStatus::ValidFinalized) {
            report.overall_status = has_warnings ? "warning" : "valid";
        } else {
            report.overall_status = "invalid";
            has_errors = true;
        }
    }

    // Output
    std::cout << generate_text_report(report);

    if (!json_out.empty()) {
        std::ofstream ofs(json_out);
        if (ofs) {
            ofs << generate_json_report(report);
        } else {
            std::cerr << "Error: cannot write JSON to " << json_out << "\n";
            return 1;
        }
    }

    // Invalid/corrupt always returns non-zero
    if (has_errors) return 1;
    // Strict additionally promotes warnings and partial files
    if (strict && (has_warnings || has_partial)) return 1;

    return 0;
}

static int cmd_replay(int argc, char* argv[]) {
    std::string input_dir;
    std::string session_hex;
    std::uint64_t source_id = 0;
    std::uint64_t channel_id = 0;
    std::string json_out;
    bool session_set = false;
    bool source_set = false;
    bool channel_set = false;

    for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--help") { print_replay_help(); return 0; }
        if (arg == "--input" && i + 1 < argc) { input_dir = argv[++i]; }
        else if (arg == "--session" && i + 1 < argc) { session_hex = argv[++i]; session_set = true; }
        else if (arg == "--source" && i + 1 < argc) {
            if (!parse_hex_u64_strict(argv[++i], source_id)) {
                std::cerr << "Error: invalid --source hex value\n"; return 1;
            }
            source_set = true;
        }
        else if (arg == "--channel" && i + 1 < argc) {
            if (!parse_hex_u64_strict(argv[++i], channel_id)) {
                std::cerr << "Error: invalid --channel hex value\n"; return 1;
            }
            channel_set = true;
        }
        else if (arg == "--json-out" && i + 1 < argc) { json_out = argv[++i]; }
        else {
            std::cerr << "Unknown option: " << arg << "\n";
            return 1;
        }
    }

    if (input_dir.empty()) {
        std::cerr << "Error: --input is required\n";
        return 1;
    }

    // Discover stream sets and collect all discovery issues
    std::vector<RawValidationIssue> discovery_issues;
    auto stream_sets = group_stream_sets(input_dir, discovery_issues);

    // Check for discovery errors
    bool has_discovery_errors = false;
    for (const auto& issue : discovery_issues) {
        if (issue.severity == ValidationSeverity::Error) has_discovery_errors = true;
    }

    if (stream_sets.empty()) {
        std::cerr << "Error: no valid streams found in " << input_dir << "\n";
        return 1;
    }

    // Find matching stream sets by selectors
    std::vector<StreamSetInfo*> matches;
    for (auto& ss : stream_sets) {
        bool match = true;
        if (source_set && ss.source_id != source_id) match = false;
        if (channel_set && ss.channel_id != channel_id) match = false;
        if (session_set) {
            std::uint8_t sid[16];
            if (!parse_session_id_hex(session_hex, sid)) {
                std::cerr << "Error: invalid --session hex\n";
                return 1;
            }
            if (std::memcmp(ss.session_id, sid, 16) != 0) match = false;
        }
        if (match) matches.push_back(&ss);
    }

    if (matches.empty()) {
        std::cerr << "Error: no matching stream set found\n";
        return 1;
    }

    // Any matches.size() != 1 is ambiguous (same-session different source/channel too)
    if (matches.size() != 1) {
        std::cerr << "Error: multiple stream sets match; specify --session, --source, and --channel\n";
        return 1;
    }

    // Check for partial files — block replay
    bool has_partial = false;
    for (const auto& issue : discovery_issues) {
        if (issue.code == "PARTIAL_FILE") has_partial = true;
    }
    if (has_partial) {
        std::cerr << "Error: partial files present; completeness unknown\n";
        return 1;
    }

    if (has_discovery_errors) {
        std::cerr << "Error: discovery errors found\n";
        return 1;
    }

    StreamSetInfo* target = matches[0];

    // Validate to get metadata before replay
    std::vector<RawSegmentMetadata> metas;
    std::vector<RawFooter> footers;
    std::vector<RawValidationIssue> val_issues;
    std::uint64_t first_utc = 0, last_utc = 0;
    auto val_status = validate_stream_set(target->segment_paths, metas, footers, val_issues,
                                          &first_utc, &last_utc);
    if (val_status != SegmentStatus::ValidFinalized || metas.empty()) {
        std::cerr << "Error: stream set validation failed\n";
        return 1;
    }

    RawSegmentMetadata replay_meta = metas[0];

    // Replay using replay_from_stream_set — computes SHA-256 in single streaming pass
    auto result = replay_from_stream_set(*target,
        [](const RawSegmentMetadata&, const RawPacketRecord&) -> bool {
            return true;
        });

    RawSegmentReport report;
    report.operation = "replay";
    report.format_version = "1";
    report.input_paths.push_back(input_dir);
    report.session_id_hex = session_id_hex(replay_meta.session.session_id);
    report.source_id = replay_meta.source.source_id;
    report.channel_id = replay_meta.source.channel_id;
    report.feed_group = replay_meta.session.feed_group;
    report.endpoint_role = replay_meta.session.endpoint_role;
    report.source_label = replay_meta.session.source_label;
    report.clock_domain = clock_domain_name(replay_meta.source.clock_domain);
    report.transport = transport_name(replay_meta.source.transport);
    report.source_side = source_side_name(replay_meta.source.source_side);
    report.configuration_sha256 = sha256_bytes_to_hex(replay_meta.source.configuration_sha256);
    report.templates_sha256 = sha256_bytes_to_hex(replay_meta.source.templates_sha256);
    report.endpoint_fingerprint_sha256 = sha256_bytes_to_hex(replay_meta.source.endpoint_fingerprint_sha256);
    report.stream_key = report.session_id_hex + "_src" + source_id_hex(replay_meta.source.source_id) +
                       "_ch" + channel_id_hex(replay_meta.source.channel_id);

    report.record_count = result.summary.record_count;
    report.total_payload_bytes = result.summary.total_payload_bytes;
    report.first_capture_index = result.summary.first_capture_index;
    report.last_capture_index = result.summary.last_capture_index;
    report.first_capture_utc_ns = first_utc;
    report.last_capture_utc_ns = last_utc;

    // Merge discovery issues + replay issues with source info
    auto stream_key = report.stream_key;
    for (auto& issue : discovery_issues) {
        issue.source = stream_key;
    }
    report.issues = std::move(discovery_issues);
    for (auto& issue : result.issues) {
        issue.source = stream_key;
        report.issues.push_back(std::move(issue));
    }

    if (result.status == ReplayStatus::Ok) {
        report.replay_sha256 = result.summary.replay_sha256;
        report.overall_status = "valid";
    } else {
        report.overall_status = "invalid";
    }

    std::cout << generate_text_report(report);

    if (!json_out.empty()) {
        std::ofstream ofs(json_out);
        if (ofs) {
            ofs << generate_json_report(report);
        } else {
            std::cerr << "Error: cannot write JSON to " << json_out << "\n";
            return 1;
        }
    }

    return (result.status == ReplayStatus::Ok) ? 0 : 1;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        print_help();
        return 1;
    }

    std::string command = argv[1];
    if (command == "--help" || command == "-h") {
        print_help();
        return 0;
    }
    if (command == "synth") return cmd_synth(argc, argv);
    if (command == "inspect") return cmd_inspect(argc, argv);
    if (command == "replay") return cmd_replay(argc, argv);

    std::cerr << "Unknown command: " << command << "\n";
    print_help();
    return 1;
}
