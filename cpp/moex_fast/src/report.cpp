#include "moex_fast/report.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>

namespace moex_fast {

namespace {

void json_escape(std::ostringstream& oss, const std::string& s) {
    oss << '"';
    for (char c : s) {
        switch (c) {
            case '"': oss << "\\\""; break;
            case '\\': oss << "\\\\"; break;
            case '\n': oss << "\\n"; break;
            case '\r': oss << "\\r"; break;
            case '\t': oss << "\\t"; break;
            default: oss << c;
        }
    }
    oss << '"';
}

void indent(std::ostringstream& oss, int depth) {
    for (int i = 0; i < depth; ++i) oss << "  ";
}

void emit_required_results(std::ostringstream& oss, int depth,
                           const std::vector<RequiredCheckResult>& results) {
    for (std::size_t i = 0; i < results.size(); ++i) {
        const auto& r = results[i];
        indent(oss, depth); oss << "{\n";
        indent(oss, depth + 1); oss << "\"name\": "; json_escape(oss, r.name); oss << ",\n";
        indent(oss, depth + 1); oss << "\"present\": " << (r.present ? "true" : "false") << ",\n";
        indent(oss, depth + 1); oss << "\"severity\": ";
        json_escape(oss, r.severity == Severity::Error ? "error" : "warning");
        oss << "\n";
        indent(oss, depth); oss << "}";
        if (i + 1 < results.size()) oss << ",";
        oss << "\n";
    }
}

}  // namespace

std::string report_to_json(const InspectionReport& r) {
    std::ostringstream oss;
    oss << "{\n";

    // Schema version and inspector version
    indent(oss, 1); oss << "\"schema_version\": "; json_escape(oss, r.schema_version); oss << ",\n";
    indent(oss, 1); oss << "\"inspector_version\": "; json_escape(oss, r.inspector_version); oss << ",\n";

    // Profile information
    indent(oss, 1); oss << "\"detected_profile\": "; json_escape(oss, r.detected_profile); oss << ",\n";
    indent(oss, 1); oss << "\"profile_evidence\": "; json_escape(oss, r.profile_evidence); oss << ",\n";
    indent(oss, 1); oss << "\"compatibility_status\": "; json_escape(oss, r.compatibility_status); oss << ",\n";

    // Input provenance - templates
    indent(oss, 1); oss << "\"templates_file\": {\n";
    indent(oss, 2); oss << "\"path\": "; json_escape(oss, r.templates_info.path); oss << ",\n";
    indent(oss, 2); oss << "\"file_name\": "; json_escape(oss, r.templates_info.file_name); oss << ",\n";
    indent(oss, 2); oss << "\"file_size\": " << r.templates_info.file_size << ",\n";
    indent(oss, 2); oss << "\"sha256\": "; json_escape(oss, r.templates_info.sha256); oss << ",\n";
    indent(oss, 2); oss << "\"parse_ok\": " << (r.templates_info.parse_ok ? "true" : "false") << ",\n";
    indent(oss, 2); oss << "\"validation_ok\": " << (r.templates_info.validation_ok ? "true" : "false") << "\n";
    indent(oss, 1); oss << "},\n";

    // Input provenance - configuration
    indent(oss, 1); oss << "\"configuration_file\": {\n";
    indent(oss, 2); oss << "\"path\": "; json_escape(oss, r.configuration_info.path); oss << ",\n";
    indent(oss, 2); oss << "\"file_name\": "; json_escape(oss, r.configuration_info.file_name); oss << ",\n";
    indent(oss, 2); oss << "\"file_size\": " << r.configuration_info.file_size << ",\n";
    indent(oss, 2); oss << "\"sha256\": "; json_escape(oss, r.configuration_info.sha256); oss << ",\n";
    indent(oss, 2); oss << "\"parse_ok\": " << (r.configuration_info.parse_ok ? "true" : "false") << ",\n";
    indent(oss, 2); oss << "\"validation_ok\": " << (r.configuration_info.validation_ok ? "true" : "false") << "\n";
    indent(oss, 1); oss << "},\n";

    // Required template results
    indent(oss, 1); oss << "\"required_templates\": [\n";
    emit_required_results(oss, 2, r.required_template_results);
    indent(oss, 1); oss << "],\n";

    // Required feed results
    indent(oss, 1); oss << "\"required_feeds\": [\n";
    emit_required_results(oss, 2, r.required_feed_results);
    indent(oss, 1); oss << "],\n";

    // Templates summary
    indent(oss, 1); oss << "\"templates\": [\n";
    // Sort by ID for deterministic output
    std::vector<std::size_t> tmpl_order(r.templates.size());
    for (std::size_t i = 0; i < r.templates.size(); ++i) tmpl_order[i] = i;
    std::sort(tmpl_order.begin(), tmpl_order.end(),
        [&](std::size_t a, std::size_t b) { return r.templates[a].id < r.templates[b].id; });

    for (std::size_t idx = 0; idx < tmpl_order.size(); ++idx) {
        const auto& t = r.templates[tmpl_order[idx]];
        indent(oss, 2); oss << "{\n";
        indent(oss, 3); oss << "\"id\": " << t.id << ",\n";
        indent(oss, 3); oss << "\"name\": "; json_escape(oss, t.name); oss << ",\n";
        indent(oss, 3); oss << "\"fields\": [\n";

        // Sort fields by order for deterministic output
        std::vector<std::size_t> field_order(t.fields.size());
        for (std::size_t i = 0; i < t.fields.size(); ++i) field_order[i] = i;
        std::sort(field_order.begin(), field_order.end(),
            [&](std::size_t a, std::size_t b) { return t.fields[a].order < t.fields[b].order; });

        for (std::size_t fi = 0; fi < field_order.size(); ++fi) {
            const auto& f = t.fields[field_order[fi]];
            indent(oss, 4); oss << "{\n";
            indent(oss, 5); oss << "\"order\": " << f.order << ",\n";
            indent(oss, 5); oss << "\"name\": "; json_escape(oss, f.name); oss << ",\n";
            indent(oss, 5); oss << "\"wire_type\": "; json_escape(oss, wire_type_name(f.wire_type)); oss << ",\n";
            indent(oss, 5); oss << "\"is_mandatory\": " << (f.is_mandatory ? "true" : "false");
            if (f.has_fix_tag) {
                oss << ",\n";
                indent(oss, 5); oss << "\"fix_tag\": " << f.fix_tag;
            }
            if (f.is_constant) {
                oss << ",\n";
                indent(oss, 5); oss << "\"constant_value\": "; json_escape(oss, f.constant_value);
            }
            if (f.is_sequence_length) {
                oss << ",\n";
                indent(oss, 5); oss << "\"is_sequence_length\": true";
            }
            if (!f.charset.empty()) {
                oss << ",\n";
                indent(oss, 5); oss << "\"charset\": "; json_escape(oss, f.charset);
            }
            if (!f.parent_sequence.empty()) {
                oss << ",\n";
                indent(oss, 5); oss << "\"parent_sequence\": "; json_escape(oss, f.parent_sequence);
            }
            oss << "\n";
            indent(oss, 4); oss << "}";
            if (fi + 1 < field_order.size()) oss << ",";
            oss << "\n";
        }
        indent(oss, 3); oss << "]\n";
        indent(oss, 2); oss << "}";
        if (idx + 1 < tmpl_order.size()) oss << ",";
        oss << "\n";
    }
    indent(oss, 1); oss << "],\n";

    // Feed groups — name is feedType, label is human-readable, endpoint_role is connection/type
    indent(oss, 1); oss << "\"feed_groups\": [\n";
    for (std::size_t gi = 0; gi < r.feed_groups.size(); ++gi) {
        const auto& g = r.feed_groups[gi];
        indent(oss, 2); oss << "{\n";
        indent(oss, 3); oss << "\"feedType\": "; json_escape(oss, g.name); oss << ",\n";
        indent(oss, 3); oss << "\"label\": "; json_escape(oss, g.label); oss << ",\n";
        indent(oss, 3); oss << "\"market_id\": "; json_escape(oss, g.market_id); oss << ",\n";
        indent(oss, 3); oss << "\"endpoints\": [\n";
        for (std::size_t ei = 0; ei < g.endpoints.size(); ++ei) {
            const auto& ep = g.endpoints[ei];
            indent(oss, 4); oss << "{\n";
            indent(oss, 5); oss << "\"endpoint_role\": "; json_escape(oss, ep.endpoint_role); oss << ",\n";
            indent(oss, 5); oss << "\"protocol\": "; json_escape(oss, ep.protocol); oss << ",\n";
            indent(oss, 5); oss << "\"source_ip\": "; json_escape(oss, ep.source_ip); oss << ",\n";
            indent(oss, 5); oss << "\"multicast_group\": "; json_escape(oss, ep.multicast_group); oss << ",\n";
            indent(oss, 5); oss << "\"port\": " << ep.port << ",\n";
            indent(oss, 5); oss << "\"feed_id\": "; json_escape(oss, ep.feed_id); oss << "\n";
            indent(oss, 4); oss << "}";
            if (ei + 1 < g.endpoints.size()) oss << ",";
            oss << "\n";
        }
        indent(oss, 3); oss << "]\n";
        indent(oss, 2); oss << "}";
        if (gi + 1 < r.feed_groups.size()) oss << ",";
        oss << "\n";
    }
    indent(oss, 1); oss << "],\n";

    // Issues
    indent(oss, 1); oss << "\"issues\": [\n";
    for (std::size_t i = 0; i < r.issues.size(); ++i) {
        const auto& iss = r.issues[i];
        indent(oss, 2); oss << "{\n";
        indent(oss, 3); oss << "\"severity\": ";
        json_escape(oss, iss.severity == Severity::Error ? "error" : "warning");
        oss << ",\n";
        indent(oss, 3); oss << "\"source\": ";
        json_escape(oss, iss.source == IssueSource::Template ? "template" : "configuration");
        oss << ",\n";
        indent(oss, 3); oss << "\"message\": "; json_escape(oss, iss.message); oss << "\n";
        indent(oss, 2); oss << "}";
        if (i + 1 < r.issues.size()) oss << ",";
        oss << "\n";
    }
    indent(oss, 1); oss << "],\n";

    // Overall status
    indent(oss, 1); oss << "\"overall_status\": "; json_escape(oss, r.overall_status); oss << "\n";
    oss << "}\n";

    return oss.str();
}

std::string report_to_text(const InspectionReport& r) {
    std::ostringstream oss;
    oss << "=== MOEX FAST Configuration/Template Inspector ===\n";
    oss << "Schema version: " << r.schema_version << "\n";
    oss << "Inspector version: " << r.inspector_version << "\n";
    oss << "Detected profile: " << r.detected_profile << "\n";
    oss << "Profile evidence: " << r.profile_evidence << "\n";
    oss << "Compatibility status: " << r.compatibility_status << "\n\n";

    oss << "--- Templates file ---\n";
    oss << "  Path: " << r.templates_info.path << "\n";
    oss << "  Size: " << r.templates_info.file_size << " bytes\n";
    oss << "  SHA-256: " << r.templates_info.sha256 << "\n";
    oss << "  Parse: " << (r.templates_info.parse_ok ? "OK" : "FAILED") << "\n";
    oss << "  Validation: " << (r.templates_info.validation_ok ? "OK" : "FAILED") << "\n\n";

    oss << "--- Configuration file ---\n";
    oss << "  Path: " << r.configuration_info.path << "\n";
    oss << "  Size: " << r.configuration_info.file_size << " bytes\n";
    oss << "  SHA-256: " << r.configuration_info.sha256 << "\n";
    oss << "  Parse: " << (r.configuration_info.parse_ok ? "OK" : "FAILED") << "\n";
    oss << "  Validation: " << (r.configuration_info.validation_ok ? "OK" : "FAILED") << "\n\n";

    oss << "--- Templates (" << r.templates.size() << ") ---\n";
    for (const auto& t : r.templates) {
        oss << "  [" << t.id << "] " << t.name
            << " (" << t.fields.size() << " fields)\n";
    }

    oss << "\n--- Feed Groups (" << r.feed_groups.size() << ") ---\n";
    for (const auto& g : r.feed_groups) {
        oss << "  " << g.name;
        if (!g.label.empty()) oss << " (" << g.label << ")";
        oss << " (" << g.endpoints.size() << " endpoints)\n";
        for (const auto& ep : g.endpoints) {
            oss << "    " << ep.endpoint_role << " " << ep.feed_id
                << " " << ep.protocol << " " << ep.multicast_group
                << ":" << ep.port << "\n";
        }
    }

    if (!r.required_template_results.empty()) {
        oss << "\n--- Required Templates ---\n";
        for (const auto& rt : r.required_template_results) {
            oss << "  " << rt.name << ": " << (rt.present ? "FOUND" : "MISSING") << "\n";
        }
    }

    if (!r.required_feed_results.empty()) {
        oss << "\n--- Required Feeds ---\n";
        for (const auto& rf : r.required_feed_results) {
            oss << "  " << rf.name << ": " << (rf.present ? "FOUND" : "MISSING") << "\n";
        }
    }

    if (!r.issues.empty()) {
        oss << "\n--- Issues (" << r.issues.size() << ") ---\n";
        for (const auto& iss : r.issues) {
            oss << "  [" << (iss.severity == Severity::Error ? "ERROR" : "WARN ")
                << "][" << (iss.source == IssueSource::Template ? "TMPL" : "CONF")
                << "] " << iss.message << "\n";
        }
    }

    oss << "\nOverall status: " << r.overall_status << "\n";
    return oss.str();
}

std::string write_json_report(const InspectionReport& report, const std::string& path) {
    std::ofstream ofs(path);
    if (!ofs) {
        return "Failed to open output file: " + path;
    }
    ofs << report_to_json(report);
    if (!ofs) {
        return "Failed to write output file: " + path;
    }
    return {};
}

}  // namespace moex_fast
