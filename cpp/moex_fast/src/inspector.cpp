#include "moex_fast/inspector.hpp"
#include "moex_fast/xml_parser.hpp"
#include "moex_fast/sha256.hpp"
#include <algorithm>
#include <fstream>
#include <set>
#include <sstream>
#include <iomanip>

namespace moex_fast {

namespace {

std::uint64_t file_size(const std::string& path) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) return 0;
    return static_cast<std::uint64_t>(f.tellg());
}

std::string filename_from_path(const std::string& path) {
    auto pos = path.find_last_of("/\\");
    return (pos == std::string::npos) ? path : path.substr(pos + 1);
}

void validate_templates(
    const std::vector<FastTemplateDescriptor>& templates,
    bool strict,
    std::vector<InspectionIssue>& issues,
    std::vector<RequiredCheckResult>& required_results) {

    static const std::set<std::uint32_t> required_ids = {
        29, 30, 31, 32, 40, 45, 46};

    std::set<std::uint32_t> present;
    for (const auto& t : templates) {
        present.insert(t.id);
    }

    for (auto rid : required_ids) {
        bool found = present.count(rid) > 0;
        Severity sev = strict ? Severity::Error : Severity::Warning;
        if (!found) {
            issues.push_back({sev, IssueSource::Template,
                "Missing required template id: " + std::to_string(rid)});
        }
        required_results.push_back({"template-" + std::to_string(rid), found, sev});
    }

    for (const auto& t : templates) {
        for (const auto& f : t.fields) {
            if (f.wire_type == WireType::Unknown) {
                issues.push_back({Severity::Warning, IssueSource::Template,
                    "Unknown wire type in template " + std::to_string(t.id) +
                    " field " + f.name});
            }
        }
    }
}

void validate_configuration(
    const std::vector<FeedGroup>& groups,
    bool strict,
    std::vector<InspectionIssue>& issues,
    std::vector<RequiredCheckResult>& required_results) {

    bool has_orders_log = false;
    bool has_fut_info = false;
    bool has_incr_a = false;
    bool has_incr_b = false;
    bool has_snap_a = false;
    bool has_snap_b = false;
    bool has_hist_replay = false;

    for (const auto& g : groups) {
        if (g.name == "ORDERS-LOG") {
            has_orders_log = true;
            for (const auto& ep : g.endpoints) {
                if (ep.feed_type == "Incremental") {
                    if (ep.feed_id == "A") has_incr_a = true;
                    if (ep.feed_id == "B") has_incr_b = true;
                }
                if (ep.feed_type == "Snapshot") {
                    if (ep.feed_id == "A") has_snap_a = true;
                    if (ep.feed_id == "B") has_snap_b = true;
                }
                if (ep.feed_type == "Historical Replay") has_hist_replay = true;
            }
        }
        if (g.name == "FUT-INFO") has_fut_info = true;
    }

    auto check = [&](bool cond, const char* msg, const char* result_name) {
        Severity sev = strict ? Severity::Error : Severity::Warning;
        if (!cond) {
            issues.push_back({sev, IssueSource::Configuration, msg});
        }
        required_results.push_back({result_name, cond, sev});
    };

    check(has_orders_log, "ORDERS-LOG feed group missing", "ORDERS-LOG");
    check(has_fut_info, "FUT-INFO feed group missing", "FUT-INFO");
    check(has_incr_a, "ORDERS-LOG Incremental A missing", "ORDERS-LOG-Incr-A");
    check(has_incr_b, "ORDERS-LOG Incremental B missing", "ORDERS-LOG-Incr-B");
    check(has_snap_a, "ORDERS-LOG Snapshot A missing", "ORDERS-LOG-Snap-A");
    check(has_snap_b, "ORDERS-LOG Snapshot B missing", "ORDERS-LOG-Snap-B");
    check(has_hist_replay, "ORDERS-LOG Historical Replay missing", "ORDERS-LOG-HistReplay");

    // Validate ports (already parsed strictly in xml_parser, but check for zero)
    for (const auto& g : groups) {
        for (const auto& ep : g.endpoints) {
            if (ep.port == 0) {
                issues.push_back({Severity::Error, IssueSource::Configuration,
                    "Zero port in " + g.name + " endpoint"});
            }
        }
    }

    // Check for duplicate endpoints
    std::set<std::string> seen;
    for (const auto& g : groups) {
        for (const auto& ep : g.endpoints) {
            std::string key = ep.multicast_group + ":" +
                std::to_string(ep.port) + ":" + ep.feed_id;
            if (seen.count(key)) {
                issues.push_back({Severity::Warning, IssueSource::Configuration,
                    "Duplicate endpoint: " + key});
            }
            seen.insert(key);
        }
    }
}

}  // namespace

InspectionReport run_inspector(const InspectorOptions& opts) {
    InspectionReport report;
    report.schema_version = "1.0";
    report.inspector_version = "0.1.0";

    // Compute file provenance
    report.templates_info.path = opts.templates_path;
    report.templates_info.file_name = filename_from_path(opts.templates_path);
    report.templates_info.file_size = file_size(opts.templates_path);
    report.templates_info.sha256 = compute_sha256_file(opts.templates_path);

    report.configuration_info.path = opts.configuration_path;
    report.configuration_info.file_name = filename_from_path(opts.configuration_path);
    report.configuration_info.file_size = file_size(opts.configuration_path);
    report.configuration_info.sha256 = compute_sha256_file(opts.configuration_path);

    // Parse templates (issues get IssueSource::Template)
    report.templates_info.parse_ok = parse_templates_xml(
        opts.templates_path, report.templates, report.issues);

    // Parse configuration (issues get IssueSource::Configuration)
    report.configuration_info.parse_ok = parse_configuration_xml(
        opts.configuration_path, report.feed_groups, report.issues);

    // Validate templates independently
    if (report.templates_info.parse_ok) {
        std::size_t issue_start = report.issues.size();
        validate_templates(report.templates, opts.strict,
                          report.issues, report.required_template_results);
        // Check only template-sourced issues for templates validation_ok
        report.templates_info.validation_ok = true;
        for (std::size_t i = issue_start; i < report.issues.size(); ++i) {
            if (report.issues[i].source == IssueSource::Template &&
                report.issues[i].severity == Severity::Error) {
                report.templates_info.validation_ok = false;
                break;
            }
        }
    }

    // Validate configuration independently
    if (report.configuration_info.parse_ok) {
        std::size_t issue_start = report.issues.size();
        validate_configuration(report.feed_groups, opts.strict,
                              report.issues, report.required_feed_results);
        // Check only configuration-sourced issues for configuration validation_ok
        report.configuration_info.validation_ok = true;
        for (std::size_t i = issue_start; i < report.issues.size(); ++i) {
            if (report.issues[i].source == IssueSource::Configuration &&
                report.issues[i].severity == Severity::Error) {
                report.configuration_info.validation_ok = false;
                break;
            }
        }
    }

    // Also check pre-existing parse issues for validation_ok
    if (report.templates_info.parse_ok && report.templates_info.validation_ok) {
        for (const auto& iss : report.issues) {
            if (iss.source == IssueSource::Template && iss.severity == Severity::Error) {
                report.templates_info.validation_ok = false;
                break;
            }
        }
    }
    if (report.configuration_info.parse_ok && report.configuration_info.validation_ok) {
        for (const auto& iss : report.issues) {
            if (iss.source == IssueSource::Configuration && iss.severity == Severity::Error) {
                report.configuration_info.validation_ok = false;
                break;
            }
        }
    }

    // Determine overall status
    bool has_error = false;
    bool has_warning = false;
    for (const auto& iss : report.issues) {
        if (iss.severity == Severity::Error) has_error = true;
        if (iss.severity == Severity::Warning) has_warning = true;
    }

    if (has_error) {
        report.overall_status = "invalid";
    } else if (has_warning) {
        report.overall_status = "warning";
    } else {
        report.overall_status = "valid";
    }

    return report;
}

}  // namespace moex_fast
