#include "moex_raw/raw_types.hpp"
#include "moex_raw/raw_segment.hpp"
#include "moex_raw/raw_replay.hpp"
#include "moex_raw/raw_report.hpp"
#include "moex_raw/sha256.hpp"
#include "moex_raw/crc32c.hpp"
#include <iostream>
#include <fstream>
#include <cstring>
#include <filesystem>
#include <vector>
#include <string>

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
              << "  --source <hex16>       Source ID to select (16 hex chars)\n"
              << "  --channel <hex16>      Channel ID to select (16 hex chars)\n"
              << "  --json-out <path>      Write JSON report to file\n"
              << "  --help                 Show this help\n";
}

static std::uint64_t parse_hex_u64(const std::string& s) {
    return std::strtoull(s.c_str(), nullptr, 16);
}

static bool parse_u64(const std::string& s, std::uint64_t& out) {
    char* end = nullptr;
    out = std::strtoull(s.c_str(), &end, 10);
    return end == s.c_str() + s.size();
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
        else if (arg == "--source" && i + 1 < argc) { source_id = parse_hex_u64(argv[++i]); }
        else if (arg == "--channel" && i + 1 < argc) { channel_id = parse_hex_u64(argv[++i]); }
        else if (arg == "--seed" && i + 1 < argc) { parse_u64(argv[++i], seed); }
        else if (arg == "--records" && i + 1 < argc) { parse_u64(argv[++i], records_per_seg); }
        else if (arg == "--segments" && i + 1 < argc) { parse_u64(argv[++i], num_segments); }
        else if (arg == "--payload-size" && i + 1 < argc) { parse_u64(argv[++i], payload_size); }
        else if (arg == "--max-records" && i + 1 < argc) { parse_u64(argv[++i], max_records); }
        else if (arg == "--max-bytes" && i + 1 < argc) { parse_u64(argv[++i], max_bytes); }
        else if (arg == "--feed-group" && i + 1 < argc) { feed_group = argv[++i]; }
        else if (arg == "--endpoint-role" && i + 1 < argc) { endpoint_role = argv[++i]; }
        else {
            std::cerr << "Unknown option: " << arg << "\n";
            return 1;
        }
    }

    if (out_dir.empty()) {
        std::cerr << "Error: --out is required\n";
        return 1;
    }

    if (session_hex.size() != 32) {
        std::cerr << "Error: --session must be 32 hex chars\n";
        return 1;
    }

    if (payload_size > kMaxPayloadSize) {
        std::cerr << "Error: payload size exceeds 1 MiB\n";
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

    RawSegmentRotationPolicy policy;
    policy.max_records_per_segment = max_records;
    policy.max_segment_bytes = max_bytes;

    RawSegmentWriter writer(meta, out_dir, policy);

    auto err = writer.open();
    if (!err.empty()) {
        std::cerr << "Error opening writer: " << err << "\n";
        return 1;
    }

    std::uint64_t capture_index = 0;
    for (std::uint64_t seg = 0; seg < num_segments; ++seg) {
        for (std::uint64_t rec = 0; rec < records_per_seg; ++rec) {
            RawPacketRecord packet;
            packet.record_flags = kRecordFlagUtcValid;
            packet.capture_index = capture_index;
            packet.capture_utc_ns = meta.created_utc_ns + capture_index * 1000000;
            packet.capture_monotonic_ns = capture_index * 1000000;
            packet.payload = make_synthetic_payload(seed + capture_index, static_cast<std::size_t>(payload_size));

            err = writer.append(packet);
            if (!err.empty()) {
                std::cerr << "Error appending record: " << err << "\n";
                return 1;
            }
            capture_index++;
        }

        if (seg < num_segments - 1) {
            err = writer.finalize();
            if (!err.empty()) {
                std::cerr << "Error finalizing segment: " << err << "\n";
                return 1;
            }

            // Prepare for next segment — need to update metadata
            meta.segment_index = writer.current_segment_index();
            meta.start_capture_index = writer.next_capture_index();
            // Writer handles this internally via rotation
        }
    }

    // Finalize last segment if not already finalized by rotation
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

    if (std::filesystem::is_directory(input_path)) {
        auto stream_sets = group_stream_sets(input_path);
        report.input_paths.push_back(input_path);

        if (stream_sets.empty()) {
            report.overall_status = "invalid";
            report.issues.push_back({ValidationSeverity::Error, "NO_STREAMS", "No valid streams found"});
        } else {
            for (const auto& ss : stream_sets) {
                std::vector<RawSegmentMetadata> metas;
                std::vector<RawFooter> footers;
                std::vector<RawValidationIssue> issues;

                auto status = validate_stream_set(ss.segment_paths, metas, footers, issues);

                report.session_id_hex = session_id_hex(ss.session_id);
                report.stream_key = report.session_id_hex + "_src" + source_id_hex(ss.source_id) +
                                   "_ch" + channel_id_hex(ss.channel_id);

                if (!metas.empty()) {
                    report.feed_group = metas[0].session.feed_group;
                    report.endpoint_role = metas[0].session.endpoint_role;
                    report.source_label = metas[0].session.source_label;
                }

                for (std::size_t i = 0; i < metas.size(); ++i) {
                    report.segment_indexes.push_back(metas[i].segment_index);
                    // Get file size
                    std::error_code ec;
                    auto sz = std::filesystem::file_size(ss.segment_paths[i], ec);
                    report.segment_sizes.push_back(ec ? 0 : sz);
                }

                report.record_count = 0;
                report.total_payload_bytes = 0;
                for (const auto& f : footers) {
                    report.record_count += f.record_count;
                    report.total_payload_bytes += f.total_payload_bytes;
                    report.first_capture_index = footers.front().first_capture_index;
                    report.last_capture_index = footers.back().last_capture_index;
                }

                report.issues.insert(report.issues.end(), issues.begin(), issues.end());

                if (status == SegmentStatus::ValidFinalized) {
                    report.overall_status = "valid";
                } else if (!report.issues.empty()) {
                    report.overall_status = "invalid";
                }
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

        if (status == SegmentStatus::ValidFinalized) {
            report.overall_status = "valid";
        } else {
            report.overall_status = "invalid";
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

    if (strict && (report.overall_status == "invalid" || report.overall_status == "warning")) {
        return 1;
    }

    return 0;
}

static int cmd_replay(int argc, char* argv[]) {
    std::string input_dir;
    std::uint64_t source_id = 0;
    std::uint64_t channel_id = 0;
    std::string json_out;
    bool source_set = false;
    bool channel_set = false;

    for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--help") { print_replay_help(); return 0; }
        if (arg == "--input" && i + 1 < argc) { input_dir = argv[++i]; }
        else if (arg == "--source" && i + 1 < argc) { source_id = parse_hex_u64(argv[++i]); source_set = true; }
        else if (arg == "--channel" && i + 1 < argc) { channel_id = parse_hex_u64(argv[++i]); channel_set = true; }
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

    // Check if directory has multiple streams
    auto stream_sets = group_stream_sets(input_dir);
    if (stream_sets.empty()) {
        std::cerr << "Error: no valid streams found in " << input_dir << "\n";
        return 1;
    }

    if (stream_sets.size() > 1 && (!source_set || !channel_set)) {
        std::cerr << "Error: multiple stream sets found; specify --source and --channel\n";
        return 1;
    }

    if (!source_set || !channel_set) {
        // Use the only stream set
        source_id = stream_sets[0].source_id;
        channel_id = stream_sets[0].channel_id;
    }

    // Collect all records for replay digest
    std::vector<RawPacketRecord> all_records;
    RawSegmentMetadata replay_meta;

    auto result = replay_from_directory(input_dir, source_id, channel_id,
        [&](const RawSegmentMetadata& meta, const RawPacketRecord& rec) -> bool {
            if (all_records.empty()) replay_meta = meta;
            all_records.push_back(rec);
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

    report.record_count = result.summary.record_count;
    report.total_payload_bytes = result.summary.total_payload_bytes;
    report.first_capture_index = result.summary.first_capture_index;
    report.last_capture_index = result.summary.last_capture_index;

    report.issues = std::move(result.issues);

    if (result.status == ReplayStatus::Ok) {
        report.replay_sha256 = compute_replay_sha256(replay_meta, all_records);
        report.overall_status = "valid";
    } else if (result.status == ReplayStatus::AmbiguousStream) {
        report.overall_status = "invalid";
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

    return result.status == ReplayStatus::Ok ? 0 : 1;
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
