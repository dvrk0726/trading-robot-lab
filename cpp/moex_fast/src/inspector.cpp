#include "moex_fast/inspector.hpp"
#include "moex_fast/xml_parser.hpp"
#include <algorithm>
#include <fstream>
#include <set>
#include <sstream>
#include <iomanip>

#ifdef _WIN32
#include <windows.h>
#include <bcrypt.h>
#pragma comment(lib, "bcrypt.lib")
#else
#include <openssl/sha.h>
#endif

namespace moex_fast {

namespace {

std::string compute_sha256(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) return {};

#ifdef _WIN32
    BCRYPT_ALG_HANDLE hAlg = nullptr;
    BCRYPT_HASH_HANDLE hHash = nullptr;
    NTSTATUS status;

    status = BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA256_ALGORITHM,
                                          nullptr, 0);
    if (!BCRYPT_SUCCESS(status)) return {};

    status = BCryptCreateHash(hAlg, &hHash, nullptr, 0, nullptr, 0, 0);
    if (!BCRYPT_SUCCESS(status)) {
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return {};
    }

    char buf[4096];
    while (file.read(buf, sizeof(buf)) || file.gcount() > 0) {
        status = BCryptHashData(hHash,
            reinterpret_cast<PUCHAR>(buf),
            static_cast<ULONG>(file.gcount()), 0);
        if (!BCRYPT_SUCCESS(status)) {
            BCryptDestroyHash(hHash);
            BCryptCloseAlgorithmProvider(hAlg, 0);
            return {};
        }
    }

    unsigned char hash[32];
    status = BCryptFinishHash(hHash, hash, 32, 0);
    BCryptDestroyHash(hHash);
    BCryptCloseAlgorithmProvider(hAlg, 0);
    if (!BCRYPT_SUCCESS(status)) return {};
#else
    unsigned char hash[32];
    SHA256_CTX ctx;
    SHA256_Init(&ctx);
    char buf[4096];
    while (file.read(buf, sizeof(buf)) || file.gcount() > 0) {
        SHA256_Update(&ctx, buf, file.gcount());
    }
    SHA256_Final(hash, &ctx);
#endif

    std::ostringstream oss;
    for (int i = 0; i < 32; ++i) {
        oss << std::hex << std::setw(2) << std::setfill('0')
            << static_cast<int>(hash[i]);
    }
    return oss.str();
}

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
    std::vector<InspectionIssue>& issues) {

    static const std::set<std::uint32_t> required_ids = {
        29, 30, 31, 32, 40, 45, 46};

    std::set<std::uint32_t> present;
    for (const auto& t : templates) {
        present.insert(t.id);
    }

    for (auto rid : required_ids) {
        if (!present.count(rid)) {
            Severity sev = strict ? Severity::Error : Severity::Warning;
            issues.push_back({sev,
                "Missing required template id: " + std::to_string(rid)});
        }
    }

    for (const auto& t : templates) {
        for (const auto& f : t.fields) {
            if (f.wire_type == WireType::Unknown) {
                issues.push_back({Severity::Warning,
                    "Unknown wire type in template " + std::to_string(t.id) +
                    " field " + f.name});
            }
        }
    }
}

void validate_configuration(
    const std::vector<FeedGroup>& groups,
    bool strict,
    std::vector<InspectionIssue>& issues) {

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
                if (g.feed_type == "Incremental") {
                    if (ep.feed_id == "A") has_incr_a = true;
                    if (ep.feed_id == "B") has_incr_b = true;
                }
                if (g.feed_type == "Snapshot") {
                    if (ep.feed_id == "A") has_snap_a = true;
                    if (ep.feed_id == "B") has_snap_b = true;
                }
                if (g.feed_type == "Historical Replay") has_hist_replay = true;
            }
        }
        if (g.name == "FUT-INFO") has_fut_info = true;
    }

    auto check = [&](bool cond, const char* msg) {
        if (!cond) {
            Severity sev = strict ? Severity::Error : Severity::Warning;
            issues.push_back({sev, msg});
        }
    };

    check(has_orders_log, "ORDERS-LOG feed group missing");
    check(has_fut_info, "FUT-INFO feed group missing");
    check(has_incr_a, "ORDERS-LOG Incremental A missing");
    check(has_incr_b, "ORDERS-LOG Incremental B missing");
    check(has_snap_a, "ORDERS-LOG Snapshot A missing");
    check(has_snap_b, "ORDERS-LOG Snapshot B missing");
    check(has_hist_replay, "ORDERS-LOG Historical Replay missing");

    // Validate ports
    for (const auto& g : groups) {
        for (const auto& ep : g.endpoints) {
            if (ep.port == 0) {
                issues.push_back({Severity::Error,
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
                issues.push_back({Severity::Warning,
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
    report.templates_info.sha256 = compute_sha256(opts.templates_path);

    report.configuration_info.path = opts.configuration_path;
    report.configuration_info.file_name = filename_from_path(opts.configuration_path);
    report.configuration_info.file_size = file_size(opts.configuration_path);
    report.configuration_info.sha256 = compute_sha256(opts.configuration_path);

    // Parse templates
    report.templates_info.parse_ok = parse_templates_xml(
        opts.templates_path, report.templates, report.issues);

    // Parse configuration
    report.configuration_info.parse_ok = parse_configuration_xml(
        opts.configuration_path, report.feed_groups, report.issues);

    // Validate
    if (report.templates_info.parse_ok) {
        validate_templates(report.templates, opts.strict, report.issues);
        report.templates_info.validation_ok = true;
        for (const auto& iss : report.issues) {
            if (iss.severity == Severity::Error) {
                report.templates_info.validation_ok = false;
                break;
            }
        }
    }

    if (report.configuration_info.parse_ok) {
        validate_configuration(report.feed_groups, opts.strict, report.issues);
        report.configuration_info.validation_ok = true;
        for (const auto& iss : report.issues) {
            if (iss.severity == Severity::Error) {
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
