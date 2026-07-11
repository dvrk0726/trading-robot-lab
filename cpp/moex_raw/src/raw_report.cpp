#include "moex_raw/raw_report.hpp"
#include "moex_raw/raw_types.hpp"
#include <sstream>
#include <iomanip>

namespace moex_raw {

namespace {

std::string json_escape(const std::string& s) {
    std::ostringstream oss;
    oss << '"';
    for (char c : s) {
        switch (c) {
            case '"': oss << "\\\""; break;
            case '\\': oss << "\\\\"; break;
            case '\n': oss << "\\n"; break;
            case '\r': oss << "\\r"; break;
            case '\t': oss << "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    oss << "\\u" << std::hex << std::setw(4) << std::setfill('0')
                        << static_cast<int>(static_cast<unsigned char>(c));
                } else {
                    oss << c;
                }
        }
    }
    oss << '"';
    return oss.str();
}

std::string indent(int level) {
    return std::string(level * 2, ' ');
}

static void emit_stream_summary_json(std::ostringstream& oss, const RawStreamSummary& ss, int il,
                              const std::string& nl, const std::string& sep) {
    oss << indent(il) << "{" << nl;
    oss << indent(il + 1) << json_escape("session_id") << ": " << json_escape(ss.session_id_hex) << sep;
    oss << indent(il + 1) << json_escape("source_id") << ": " << ss.source_id << sep;
    oss << indent(il + 1) << json_escape("channel_id") << ": " << ss.channel_id << sep;
    oss << indent(il + 1) << json_escape("feed_group") << ": " << json_escape(ss.feed_group) << sep;
    oss << indent(il + 1) << json_escape("endpoint_role") << ": " << json_escape(ss.endpoint_role) << sep;
    oss << indent(il + 1) << json_escape("source_label") << ": " << json_escape(ss.source_label) << sep;
    oss << indent(il + 1) << json_escape("clock_domain") << ": " << json_escape(ss.clock_domain) << sep;
    oss << indent(il + 1) << json_escape("transport") << ": " << json_escape(ss.transport) << sep;
    oss << indent(il + 1) << json_escape("source_side") << ": " << json_escape(ss.source_side) << sep;
    oss << indent(il + 1) << json_escape("configuration_sha256") << ": " << json_escape(ss.configuration_sha256) << sep;
    oss << indent(il + 1) << json_escape("templates_sha256") << ": " << json_escape(ss.templates_sha256) << sep;
    oss << indent(il + 1) << json_escape("endpoint_fingerprint_sha256") << ": " << json_escape(ss.endpoint_fingerprint_sha256) << sep;
    oss << indent(il + 1) << json_escape("stream_key") << ": " << json_escape(ss.stream_key) << sep;

    oss << indent(il + 1) << json_escape("segment_indexes") << ": [";
    for (std::size_t i = 0; i < ss.segment_indexes.size(); ++i) {
        if (i > 0) oss << ", ";
        oss << ss.segment_indexes[i];
    }
    oss << "]" << sep;

    oss << indent(il + 1) << json_escape("segment_sizes") << ": [";
    for (std::size_t i = 0; i < ss.segment_sizes.size(); ++i) {
        if (i > 0) oss << ", ";
        oss << ss.segment_sizes[i];
    }
    oss << "]" << sep;

    // Per-segment hashes
    oss << indent(il + 1) << json_escape("segment_content_sha256") << ": [";
    for (std::size_t i = 0; i < ss.segment_content_sha256.size(); ++i) {
        if (i > 0) oss << ", ";
        oss << json_escape(ss.segment_content_sha256[i]);
    }
    oss << "]" << sep;

    oss << indent(il + 1) << json_escape("segment_file_sha256") << ": [";
    for (std::size_t i = 0; i < ss.segment_file_sha256.size(); ++i) {
        if (i > 0) oss << ", ";
        oss << json_escape(ss.segment_file_sha256[i]);
    }
    oss << "]" << sep;

    oss << indent(il + 1) << json_escape("content_sha256") << ": " << json_escape(ss.content_sha256) << sep;
    oss << indent(il + 1) << json_escape("file_sha256") << ": " << json_escape(ss.file_sha256) << sep;
    oss << indent(il + 1) << json_escape("record_count") << ": " << ss.record_count << sep;
    oss << indent(il + 1) << json_escape("total_payload_bytes") << ": " << ss.total_payload_bytes << sep;
    oss << indent(il + 1) << json_escape("first_capture_index") << ": " << ss.first_capture_index << sep;
    oss << indent(il + 1) << json_escape("last_capture_index") << ": " << ss.last_capture_index << sep;
    oss << indent(il + 1) << json_escape("first_capture_utc_ns") << ": " << ss.first_capture_utc_ns << sep;
    oss << indent(il + 1) << json_escape("last_capture_utc_ns") << ": " << ss.last_capture_utc_ns << sep;
    oss << indent(il + 1) << json_escape("status") << ": " << json_escape(ss.status);
    oss << nl << indent(il) << "}";
}

}  // namespace

std::string generate_json_report(const RawSegmentReport& report, bool pretty) {
    std::ostringstream oss;
    int il = pretty ? 1 : 0;
    std::string nl = pretty ? "\n" : "";
    std::string sep = pretty ? ",\n" : ",";

    oss << "{" << nl;

    // Schema/tool/format versions
    oss << indent(il) << json_escape("schema_version") << ": " << json_escape(report.schema_version) << sep;
    oss << indent(il) << json_escape("tool_version") << ": " << json_escape(report.tool_version) << sep;
    oss << indent(il) << json_escape("format_version") << ": " << json_escape(report.format_version) << sep;
    oss << indent(il) << json_escape("operation") << ": " << json_escape(report.operation) << sep;

    // Input paths
    oss << indent(il) << json_escape("input_paths") << ": [";
    for (std::size_t i = 0; i < report.input_paths.size(); ++i) {
        if (i > 0) oss << ", ";
        oss << json_escape(report.input_paths[i]);
    }
    oss << "]" << sep;

    // Session/source metadata
    oss << indent(il) << json_escape("session_id") << ": " << json_escape(report.session_id_hex) << sep;
    oss << indent(il) << json_escape("source_id") << ": " << report.source_id << sep;
    oss << indent(il) << json_escape("channel_id") << ": " << report.channel_id << sep;
    oss << indent(il) << json_escape("feed_group") << ": " << json_escape(report.feed_group) << sep;
    oss << indent(il) << json_escape("endpoint_role") << ": " << json_escape(report.endpoint_role) << sep;
    oss << indent(il) << json_escape("source_label") << ": " << json_escape(report.source_label) << sep;
    oss << indent(il) << json_escape("clock_domain") << ": " << json_escape(report.clock_domain) << sep;
    oss << indent(il) << json_escape("transport") << ": " << json_escape(report.transport) << sep;
    oss << indent(il) << json_escape("source_side") << ": " << json_escape(report.source_side) << sep;
    oss << indent(il) << json_escape("configuration_sha256") << ": " << json_escape(report.configuration_sha256) << sep;
    oss << indent(il) << json_escape("templates_sha256") << ": " << json_escape(report.templates_sha256) << sep;
    oss << indent(il) << json_escape("endpoint_fingerprint_sha256") << ": " << json_escape(report.endpoint_fingerprint_sha256) << sep;
    oss << indent(il) << json_escape("stream_key") << ": " << json_escape(report.stream_key) << sep;

    // Segment indexes
    oss << indent(il) << json_escape("segment_indexes") << ": [";
    for (std::size_t i = 0; i < report.segment_indexes.size(); ++i) {
        if (i > 0) oss << ", ";
        oss << report.segment_indexes[i];
    }
    oss << "]" << sep;

    // Segment sizes
    oss << indent(il) << json_escape("segment_sizes") << ": [";
    for (std::size_t i = 0; i < report.segment_sizes.size(); ++i) {
        if (i > 0) oss << ", ";
        oss << report.segment_sizes[i];
    }
    oss << "]" << sep;

    // Hashes
    oss << indent(il) << json_escape("content_sha256") << ": " << json_escape(report.content_sha256) << sep;
    oss << indent(il) << json_escape("file_sha256") << ": " << json_escape(report.file_sha256) << sep;

    // Counts
    oss << indent(il) << json_escape("record_count") << ": " << report.record_count << sep;
    oss << indent(il) << json_escape("total_payload_bytes") << ": " << report.total_payload_bytes << sep;
    oss << indent(il) << json_escape("first_capture_index") << ": " << report.first_capture_index << sep;
    oss << indent(il) << json_escape("last_capture_index") << ": " << report.last_capture_index << sep;
    oss << indent(il) << json_escape("first_capture_utc_ns") << ": " << report.first_capture_utc_ns << sep;
    oss << indent(il) << json_escape("last_capture_utc_ns") << ": " << report.last_capture_utc_ns << sep;

    // Issues
    oss << indent(il) << json_escape("issues") << ": [";
    for (std::size_t i = 0; i < report.issues.size(); ++i) {
        if (i > 0) oss << ", ";
        oss << nl << indent(il + 1) << "{";
        oss << nl << indent(il + 2) << json_escape("severity") << ": "
            << json_escape(report.issues[i].severity == ValidationSeverity::Warning ? "warning" : "error") << ",";
        oss << nl << indent(il + 2) << json_escape("code") << ": " << json_escape(report.issues[i].code) << ",";
        oss << nl << indent(il + 2) << json_escape("message") << ": " << json_escape(report.issues[i].message);
        if (!report.issues[i].source.empty()) {
            oss << "," << nl << indent(il + 2) << json_escape("source") << ": " << json_escape(report.issues[i].source);
        }
        if (!report.issues[i].path.empty()) {
            oss << "," << nl << indent(il + 2) << json_escape("path") << ": " << json_escape(report.issues[i].path);
        }
        oss << nl << indent(il + 1) << "}";
    }
    if (!report.issues.empty()) oss << nl << indent(il);
    oss << "]" << sep;

    // Replay digest
    oss << indent(il) << json_escape("replay_sha256") << ": " << json_escape(report.replay_sha256) << sep;

    // Per-stream summaries
    oss << indent(il) << json_escape("stream_sets") << ": [";
    for (std::size_t i = 0; i < report.stream_sets.size(); ++i) {
        if (i > 0) oss << ",";
        oss << nl;
        emit_stream_summary_json(oss, report.stream_sets[i], il + 1, nl, sep);
    }
    if (!report.stream_sets.empty()) oss << nl << indent(il);
    oss << "]" << sep;

    // Overall status
    oss << indent(il) << json_escape("overall_status") << ": " << json_escape(report.overall_status);

    oss << nl << "}";
    return oss.str();
}

std::string generate_text_report(const RawSegmentReport& report) {
    std::ostringstream oss;

    oss << "=== MXRaw Segment Report ===\n";
    oss << "Format Version: " << report.format_version << "\n";
    oss << "Operation: " << report.operation << "\n";
    oss << "Session ID: " << report.session_id_hex << "\n";
    oss << "Source ID: " << report.source_id << "\n";
    oss << "Channel ID: " << report.channel_id << "\n";
    oss << "Feed Group: " << report.feed_group << "\n";
    oss << "Endpoint Role: " << report.endpoint_role << "\n";
    oss << "Source Label: " << report.source_label << "\n";
    oss << "Clock Domain: " << report.clock_domain << "\n";
    oss << "Transport: " << report.transport << "\n";
    oss << "Source Side: " << report.source_side << "\n";
    oss << "Configuration SHA-256: " << report.configuration_sha256 << "\n";
    oss << "Templates SHA-256: " << report.templates_sha256 << "\n";
    oss << "Endpoint Fingerprint SHA-256: " << report.endpoint_fingerprint_sha256 << "\n";
    oss << "Stream Key: " << report.stream_key << "\n\n";

    oss << "Segments: " << report.segment_indexes.size() << "\n";
    for (std::size_t i = 0; i < report.segment_indexes.size(); ++i) {
        oss << "  [" << i << "] index=" << report.segment_indexes[i]
            << " size=" << report.segment_sizes[i] << "\n";
    }
    oss << "\n";

    oss << "Records: " << report.record_count << "\n";
    oss << "Payload Bytes: " << report.total_payload_bytes << "\n";
    oss << "First Capture Index: " << report.first_capture_index << "\n";
    oss << "Last Capture Index: " << report.last_capture_index << "\n";
    oss << "First Capture UTC ns: " << report.first_capture_utc_ns << "\n";
    oss << "Last Capture UTC ns: " << report.last_capture_utc_ns << "\n";
    oss << "Content SHA-256: " << report.content_sha256 << "\n";
    oss << "File SHA-256: " << report.file_sha256 << "\n";

    if (!report.replay_sha256.empty()) {
        oss << "Replay SHA-256: " << report.replay_sha256 << "\n";
    }

    // Per-stream summaries
    if (!report.stream_sets.empty()) {
        oss << "\n=== Stream Sets ===\n";
        for (std::size_t si = 0; si < report.stream_sets.size(); ++si) {
            const auto& ss = report.stream_sets[si];
            oss << "\nStream " << si << ": " << ss.stream_key << "\n";
            oss << "  Session ID: " << ss.session_id_hex << "\n";
            oss << "  Source ID: " << ss.source_id << "  Channel ID: " << ss.channel_id << "\n";
            oss << "  Feed Group: " << ss.feed_group << "\n";
            oss << "  Endpoint Role: " << ss.endpoint_role << "\n";
            oss << "  Source Label: " << ss.source_label << "\n";
            oss << "  Clock Domain: " << ss.clock_domain << "\n";
            oss << "  Transport: " << ss.transport << "\n";
            oss << "  Source Side: " << ss.source_side << "\n";
            oss << "  Configuration SHA-256: " << ss.configuration_sha256 << "\n";
            oss << "  Templates SHA-256: " << ss.templates_sha256 << "\n";
            oss << "  Endpoint Fingerprint SHA-256: " << ss.endpoint_fingerprint_sha256 << "\n";
            oss << "  Segments: " << ss.segment_indexes.size() << "\n";
            for (std::size_t i = 0; i < ss.segment_indexes.size(); ++i) {
                oss << "    [" << i << "] index=" << ss.segment_indexes[i]
                    << " size=" << ss.segment_sizes[i] << "\n";
            }
            oss << "  Records: " << ss.record_count << "\n";
            oss << "  Payload Bytes: " << ss.total_payload_bytes << "\n";
            oss << "  First Capture Index: " << ss.first_capture_index << "\n";
            oss << "  Last Capture Index: " << ss.last_capture_index << "\n";
            oss << "  Content SHA-256: " << ss.content_sha256 << "\n";
            oss << "  File SHA-256: " << ss.file_sha256 << "\n";
            oss << "  Status: " << ss.status << "\n";
        }
    }

    oss << "\nIssues: " << report.issues.size() << "\n";
    for (const auto& issue : report.issues) {
        oss << "  [" << (issue.severity == ValidationSeverity::Warning ? "WARN" : "ERR ")
            << "] " << issue.code << ": " << issue.message << "\n";
    }

    oss << "\nOverall Status: " << report.overall_status << "\n";
    return oss.str();
}

}  // namespace moex_raw
