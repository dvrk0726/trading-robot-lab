#include "moex_raw/raw_types.hpp"
#include "moex_raw/raw_segment.hpp"
#include "moex_raw/raw_replay.hpp"
#include "moex_raw/raw_report.hpp"
#include "moex_raw/sha256.hpp"
#include "moex_raw/crc32c.hpp"
#include "moex_raw/endian.hpp"
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

    // Build metadata
    RawSegmentMetadata meta;
    if (!parse_session_id_hex(session_hex, meta.session.session_id)) {
        std::cerr << "Error: invalid session ID hex\n";
        return 1;
    }
    meta.session.feed_group = feed_group;
    meta.session.endpoint_role = endpoint_role;
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

    std::uint64_t total_records = records_per_seg * num_segments;
    for (std::uint64_t i = 0; i < total_records; ++i) {
        RawPacketRecord packet;
        packet.record_flags = kRecordFlagUtcValid;
        packet.capture_index = i;
        packet.capture_utc_ns = meta.created_utc_ns + i * 1000000;
        packet.capture_monotonic_ns = i * 1000000;
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

    bool has_errors = false;
    bool has_warnings = false;

    if (std::filesystem::is_directory(input_path)) {
        std::vector<RawValidationIssue> discovery_issues;
        auto stream_sets = group_stream_sets(input_path, discovery_issues);
        report.input_paths.push_back(input_path);

        // Include discovery issues (unreadable, malformed, partial, etc.)
        report.issues.insert(report.issues.end(), discovery_issues.begin(), discovery_issues.end());
        for (const auto& issue : discovery_issues) {
            if (issue.severity == ValidationSeverity::Error) has_errors = true;
            if (issue.severity == ValidationSeverity::Warning) has_warnings = true;
        }

        if (stream_sets.empty()) {
            report.overall_status = "invalid";
            report.issues.push_back({ValidationSeverity::Error, "NO_STREAMS", "No valid streams found"});
            has_errors = true;
        } else {
            bool first_stream = true;
            for (const auto& ss : stream_sets) {
                std::vector<RawSegmentMetadata> metas;
                std::vector<RawFooter> footers;
                std::vector<RawValidationIssue> issues;

                auto status = validate_stream_set(ss.segment_paths, metas, footers, issues);

                // Don't overwrite results from previous streams
                if (first_stream) {
                    report.session_id_hex = session_id_hex(ss.session_id);
                    report.stream_key = report.session_id_hex + "_src" + source_id_hex(ss.source_id) +
                                       "_ch" + channel_id_hex(ss.channel_id);

                    if (!metas.empty()) {
                        report.feed_group = metas[0].session.feed_group;
                        report.endpoint_role = metas[0].session.endpoint_role;
                        report.source_label = metas[0].session.source_label;
                    }
                    first_stream = false;
                }

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
            } else if (has_warnings) {
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

        auto status = validate_segment(input_path, meta, footer, issues, content_hex, file_hex);

        report.session_id_hex = session_id_hex(meta.session.session_id);
        report.feed_group = meta.session.feed_group;
        report.endpoint_role = meta.session.endpoint_role;
        report.source_label = meta.session.source_label;
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

        report.issues = std::move(issues);

        for (const auto& issue : report.issues) {
            if (issue.severity == ValidationSeverity::Error) has_errors = true;
            if (issue.severity == ValidationSeverity::Warning) has_warnings = true;
        }

        if (status == SegmentStatus::ValidFinalized) {
            report.overall_status = "valid";
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
    // Strict additionally promotes warnings
    if (strict && has_warnings) return 1;

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
    if (stream_sets.empty()) {
        std::cerr << "Error: no valid streams found in " << input_dir << "\n";
        return 1;
    }

    // Find matching stream sets by full key (session_id, source_id, channel_id)
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

    // Check for ambiguity: multiple different sessions
    if (matches.size() > 1) {
        bool different_sessions = false;
        for (std::size_t i = 1; i < matches.size(); ++i) {
            if (std::memcmp(matches[i]->session_id, matches[0]->session_id, 16) != 0) {
                different_sessions = true;
                break;
            }
        }
        if (different_sessions) {
            std::cerr << "Error: multiple sessions match; specify --session, --source, and --channel\n";
            return 1;
        }
    }

    StreamSetInfo* target = matches[0];

    // Validate to get metadata before replay
    std::vector<RawSegmentMetadata> metas;
    std::vector<RawFooter> footers;
    std::vector<RawValidationIssue> val_issues;
    auto val_status = validate_stream_set(target->segment_paths, metas, footers, val_issues);
    if (val_status != SegmentStatus::ValidFinalized || metas.empty()) {
        std::cerr << "Error: stream set validation failed\n";
        return 1;
    }

    RawSegmentMetadata replay_meta = metas[0];
    source_id = replay_meta.source.source_id;
    channel_id = replay_meta.source.channel_id;

    // Initialize single SHA-256 context with canonical MXREPLAY1 metadata prefix
    SHA256Ctx replay_sha_ctx;
    sha256_init(replay_sha_ctx);

    std::vector<std::uint8_t> prefix;
    write_bytes(prefix, kMagicReplay, 10);
    write_bytes(prefix, replay_meta.session.session_id, 16);
    write_u64_le(prefix, replay_meta.source.source_id);
    write_u64_le(prefix, replay_meta.source.channel_id);
    prefix.push_back(static_cast<std::uint8_t>(replay_meta.source.clock_domain));
    prefix.push_back(static_cast<std::uint8_t>(replay_meta.source.transport));
    prefix.push_back(static_cast<std::uint8_t>(replay_meta.source.source_side));
    write_bytes(prefix, replay_meta.source.configuration_sha256, 32);
    write_bytes(prefix, replay_meta.source.templates_sha256, 32);
    write_bytes(prefix, replay_meta.source.endpoint_fingerprint_sha256, 32);
    write_u16_le(prefix, static_cast<std::uint16_t>(replay_meta.session.feed_group.size()));
    write_bytes(prefix, replay_meta.session.feed_group.data(), replay_meta.session.feed_group.size());
    write_u16_le(prefix, static_cast<std::uint16_t>(replay_meta.session.endpoint_role.size()));
    write_bytes(prefix, replay_meta.session.endpoint_role.data(), replay_meta.session.endpoint_role.size());
    write_u16_le(prefix, static_cast<std::uint16_t>(replay_meta.session.source_label.size()));
    write_bytes(prefix, replay_meta.session.source_label.data(), replay_meta.session.source_label.size());
    sha256_update(replay_sha_ctx, prefix.data(), prefix.size());

    // Replay with single callback — update same SHA-256 context for each record
    std::uint64_t replay_record_count = 0;
    std::uint64_t replay_total_payload = 0;
    std::uint64_t replay_first_index = 0;
    std::uint64_t replay_last_index = 0;
    bool first_record = true;

    auto result = replay_stream(target->segment_paths, replay_meta,
        [&](const RawSegmentMetadata& /*meta*/, const RawPacketRecord& rec) -> bool {
            if (first_record) {
                replay_first_index = rec.capture_index;
                first_record = false;
            }
            replay_last_index = rec.capture_index;
            replay_record_count++;
            replay_total_payload += rec.payload.size();

            // Update same SHA-256 context with this record
            std::vector<std::uint8_t> rec_buf;
            write_u16_le(rec_buf, rec.record_flags);
            write_u64_le(rec_buf, rec.capture_index);
            write_u64_le(rec_buf, rec.capture_utc_ns);
            write_u64_le(rec_buf, rec.capture_monotonic_ns);
            write_u32_le(rec_buf, static_cast<std::uint32_t>(rec.payload.size()));
            write_bytes(rec_buf, rec.payload.data(), rec.payload.size());
            sha256_update(replay_sha_ctx, rec_buf.data(), rec_buf.size());

            return true;
        });

    RawSegmentReport report;
    report.operation = "replay";
    report.input_paths.push_back(input_dir);
    report.session_id_hex = session_id_hex(replay_meta.session.session_id);
    report.feed_group = replay_meta.session.feed_group;
    report.endpoint_role = replay_meta.session.endpoint_role;
    report.source_label = replay_meta.session.source_label;
    report.stream_key = report.session_id_hex + "_src" + source_id_hex(source_id) +
                       "_ch" + channel_id_hex(channel_id);

    report.record_count = replay_record_count;
    report.total_payload_bytes = replay_total_payload;
    report.first_capture_index = replay_first_index;
    report.last_capture_index = replay_last_index;

    // Merge discovery issues + replay issues
    bool has_discovery_errors = false;
    for (const auto& issue : discovery_issues) {
        if (issue.severity == ValidationSeverity::Error) has_discovery_errors = true;
    }
    report.issues = std::move(discovery_issues);
    for (auto& issue : result.issues) {
        report.issues.push_back(std::move(issue));
    }

    if (result.status == ReplayStatus::Ok && !has_discovery_errors) {
        // Finalize the single SHA-256 context after successful replay
        std::uint8_t hash[32];
        sha256_final(replay_sha_ctx, hash);
        report.replay_sha256 = sha256_bytes_to_hex(hash);
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

    return (result.status == ReplayStatus::Ok && !has_discovery_errors) ? 0 : 1;
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
