#include "moex_fast/inspector.hpp"
#include "moex_fast/xml_parser.hpp"
#include "moex_fast/sha256.hpp"
#include <algorithm>
#include <fstream>
#include <map>
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

// Detect profile from template ID/name evidence.
// Returns the detected profile and fills evidence with the detection rationale.
// spectra-1.29: ID 40 SecurityDefinition present, no conflicting 1.30 identities
//               (ID 48 SecurityStatus absent, no ID 47 SecurityDefinition).
// spectra-1.30: ID 47 SecurityDefinition + ID 48 SecurityStatus, no ID 40.
// ambiguous: internally inconsistent evidence (mixed 1.29/1.30 identities,
//            wrong-name ID 47/48, partial 1.30 set, etc.).
// unknown: no distinguishing SecurityDefinition/SecurityStatus evidence.
std::string detect_profile(const std::vector<FastTemplateDescriptor>& templates,
                           std::string& evidence) {
    // Build lookup by ID
    std::map<std::uint32_t, std::string> id_to_name;
    for (const auto& t : templates) {
        id_to_name[t.id] = t.name;
    }

    bool has_40_secdef = (id_to_name.count(40) && id_to_name[40] == "SecurityDefinition");
    bool has_47_secdef = (id_to_name.count(47) && id_to_name[47] == "SecurityDefinition");
    bool has_48_secstatus = (id_to_name.count(48) && id_to_name[48] == "SecurityStatus");

    // Any ID 47 present with wrong name is conflicting 1.30 evidence
    bool has_47_wrong_name = (id_to_name.count(47) && id_to_name[47] != "SecurityDefinition");
    // Any ID 48 present with wrong name is conflicting evidence
    bool has_48_wrong_name = (id_to_name.count(48) && id_to_name[48] != "SecurityStatus");

    // Collect inconsistency details
    std::string inconsistency;

    // spectra-1.29: ID 40 SecurityDefinition, no conflicting 1.30 evidence at all
    if (has_40_secdef && !has_47_secdef && !has_48_secstatus) {
        if (has_47_wrong_name || has_48_wrong_name) {
            // Wrong-name ID 47/48 exist — internally inconsistent
            std::string details;
            if (has_47_wrong_name) {
                details += "ID 47 has wrong name '" + id_to_name[47] + "' (expected SecurityDefinition)";
            }
            if (has_48_wrong_name) {
                if (!details.empty()) details += "; ";
                details += "ID 48 has wrong name '" + id_to_name[48] + "' (expected SecurityStatus)";
            }
            evidence = "ID 40 SecurityDefinition present but " + details + " — ambiguous";
            return "ambiguous";
        }
        evidence = "ID 40 SecurityDefinition present, no conflicting 1.30 identities";
        return "spectra-1.29";
    }

    // spectra-1.30: ID 47 SecurityDefinition + ID 48 SecurityStatus, no ID 40
    if (has_47_secdef && has_48_secstatus && !has_40_secdef) {
        if (has_47_wrong_name || has_48_wrong_name) {
            // This branch can't be reached since has_47_secdef/has_48_secstatus are true,
            // but be defensive.
        }
        evidence = "ID 47 SecurityDefinition and ID 48 SecurityStatus present, ID 40 absent";
        return "spectra-1.30";
    }

    // Ambiguous: both ID 40 and ID 47 named SecurityDefinition
    if (has_40_secdef && has_47_secdef) {
        std::string details = "Both ID 40 and ID 47 named SecurityDefinition";
        if (has_48_secstatus) details += "; ID 48 SecurityStatus also present";
        evidence = details + " — ambiguous";
        return "ambiguous";
    }

    // Ambiguous: ID 40 SecurityDefinition + ID 48 SecurityStatus (mixed 1.29/1.30)
    if (has_40_secdef && has_48_secstatus) {
        evidence = "ID 40 SecurityDefinition and ID 48 SecurityStatus both present — ambiguous";
        return "ambiguous";
    }

    // Ambiguous: ID 47 SecurityDefinition present but ID 48 SecurityStatus missing
    if (has_47_secdef && !has_48_secstatus) {
        evidence = "ID 47 SecurityDefinition present but ID 48 SecurityStatus missing";
        return "ambiguous";
    }

    // Ambiguous: ID 48 SecurityStatus present but ID 47 SecurityDefinition missing
    if (has_48_secstatus && !has_47_secdef) {
        evidence = "ID 48 SecurityStatus present but ID 47 SecurityDefinition missing";
        return "ambiguous";
    }

    // Ambiguous: wrong-name IDs are internally inconsistent evidence
    if (has_47_wrong_name || has_48_wrong_name) {
        std::string details;
        if (has_47_wrong_name) {
            details += "ID 47 has wrong name '" + id_to_name[47] + "'";
        }
        if (has_48_wrong_name) {
            if (!details.empty()) details += "; ";
            details += "ID 48 has wrong name '" + id_to_name[48] + "'";
        }
        evidence = details + " — internally inconsistent";
        return "ambiguous";
    }

    evidence = "No distinguishing SecurityDefinition/SecurityStatus ID evidence";
    return "unknown";
}

struct RequiredPair { std::uint32_t id; const char* name; };

// Get required pairs for a given profile.
std::vector<RequiredPair> required_pairs_for_profile(const std::string& profile) {
    if (profile == "spectra-1.30") {
        return {
            {29, "OrdersLogMessage"},
            {30, "BookMessage"},
            {31, "DefaultIncrementalRefreshMessage"},
            {32, "DefaultSnapshotMessage"},
            {47, "SecurityDefinition"},
            {45, "SecurityGroupStatus"},
            {46, "TradingSessionStatus"},
            {48, "SecurityStatus"}
        };
    }
    // Default: spectra-1.29 (includes unknown profile)
    return {
        {29, "OrdersLogMessage"},
        {30, "BookMessage"},
        {31, "DefaultIncrementalRefreshMessage"},
        {32, "DefaultSnapshotMessage"},
        {40, "SecurityDefinition"},
        {45, "SecurityGroupStatus"},
        {46, "TradingSessionStatus"}
    };
}

// Shared template checks common to all profiles (wire type validation).
void validate_templates_common(
    const std::vector<FastTemplateDescriptor>& templates,
    std::vector<InspectionIssue>& issues) {

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

// Version-specific required template pair checks.
void validate_templates_versioned(
    const std::vector<FastTemplateDescriptor>& templates,
    bool strict,
    const std::string& profile,
    std::vector<InspectionIssue>& issues,
    std::vector<RequiredCheckResult>& required_results) {

    auto required_pairs = required_pairs_for_profile(profile);

    // Build lookup by ID
    std::map<std::uint32_t, const FastTemplateDescriptor*> by_id;
    for (const auto& t : templates) {
        by_id[t.id] = &t;
    }

    for (const auto& rp : required_pairs) {
        Severity sev = strict ? Severity::Error : Severity::Warning;
        auto it = by_id.find(rp.id);
        bool found = (it != by_id.end());
        bool name_match = found && (it->second->name == rp.name);

        if (!found) {
            issues.push_back({sev, IssueSource::Template,
                "Missing required template: " + std::to_string(rp.id) + " " + rp.name});
        } else if (!name_match) {
            issues.push_back({sev, IssueSource::Template,
                "Template " + std::to_string(rp.id) + " name mismatch: expected '" +
                rp.name + "', got '" + it->second->name + "'"});
        }

        required_results.push_back({
            "template-" + std::to_string(rp.id) + "-" + rp.name,
            found && name_match, sev});
    }
}

void validate_configuration(
    const std::vector<FeedGroup>& groups,
    bool strict,
    std::vector<InspectionIssue>& issues,
    std::vector<RequiredCheckResult>& required_results) {

    // g.name is now feedType (FUT-INFO, ORDERS-LOG)
    // ep.endpoint_role is now connection/type (Incremental, Snapshot, Historical Replay, etc.)

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
                if (ep.endpoint_role == "Incremental") {
                    if (ep.feed_id == "A") has_incr_a = true;
                    if (ep.feed_id == "B") has_incr_b = true;
                }
                if (ep.endpoint_role == "Snapshot") {
                    if (ep.feed_id == "A") has_snap_a = true;
                    if (ep.feed_id == "B") has_snap_b = true;
                }
                if (ep.endpoint_role == "Historical Replay") has_hist_replay = true;
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

    // Check for duplicate endpoints using full identity:
    // group + role + protocol + source/host + destination + port + feed
    std::set<std::string> seen;
    for (const auto& g : groups) {
        for (const auto& ep : g.endpoints) {
            std::string key = g.name + "|" + ep.endpoint_role + "|" +
                ep.protocol + "|" + ep.source_ip + "|" +
                ep.multicast_group + "|" + std::to_string(ep.port) +
                "|" + ep.feed_id;
            if (seen.count(key)) {
                issues.push_back({Severity::Warning, IssueSource::Configuration,
                    "Duplicate endpoint in group '" + g.name + "': " +
                    ep.endpoint_role + " " + ep.multicast_group + ":" +
                    std::to_string(ep.port) + " feed=" + ep.feed_id});
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

    // Always run auto-detection — the actual detected profile and evidence
    // are preserved regardless of CLI override.
    std::string evidence;
    std::string detected = detect_profile(report.templates, evidence);
    report.detected_profile = detected;
    report.detection_evidence = evidence;

    // Determine requested and selected profile.
    // Auto + spectra-1.29/1.30 => select that profile.
    // Auto + ambiguous/unknown => no selected version ("none").
    // Explicit override => use the override.
    bool is_auto = opts.profile.empty() || opts.profile == "auto";
    report.requested_profile = is_auto ? "auto" : opts.profile;

    std::string profile_to_use;
    bool has_version_profile = false;
    if (is_auto) {
        if (detected == "spectra-1.29" || detected == "spectra-1.30") {
            profile_to_use = detected;
            has_version_profile = true;
        } else {
            profile_to_use = "none";
        }
    } else {
        profile_to_use = opts.profile;
        has_version_profile = true;
    }
    report.selected_profile = profile_to_use;

    // Determine compatibility_status based on artifact evidence.
    // Ambiguous or internally inconsistent artifacts are always mismatch,
    // regardless of override mode.
    if (detected == "ambiguous") {
        report.compatibility_status = "mismatch";
        report.issues.push_back({opts.strict ? Severity::Error : Severity::Warning,
            IssueSource::Template,
            "Profile detection ambiguous: " + evidence});
    } else if (detected == "unknown") {
        report.compatibility_status = "unknown";
    } else {
        // Clear detection (spectra-1.29 or spectra-1.30)
        if (!is_auto && detected != profile_to_use) {
            // Explicit override differs from clear detection
            report.compatibility_status = "mismatch";
            report.issues.push_back({opts.strict ? Severity::Error : Severity::Warning,
                IssueSource::Template,
                "Profile mismatch: requested '" + profile_to_use +
                "' but templates indicate '" + detected + "'"});
        } else {
            report.compatibility_status = "compatible";
        }
    }

    // Validate templates independently.
    // Shared checks (wire type) always run.
    // Version-specific required pair checks only run when a version profile is established.
    if (report.templates_info.parse_ok) {
        std::size_t issue_start = report.issues.size();
        validate_templates_common(report.templates, report.issues);
        if (has_version_profile) {
            validate_templates_versioned(report.templates, opts.strict, profile_to_use,
                                        report.issues, report.required_template_results);
        }
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

    // Strict mode: unresolved compatibility (unknown or mismatch) must be invalid.
    if (opts.strict && report.compatibility_status != "compatible") {
        report.overall_status = "invalid";
    } else {
        // Determine overall status from issues
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
    }

    return report;
}

}  // namespace moex_fast
