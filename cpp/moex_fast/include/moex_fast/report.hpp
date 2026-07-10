#pragma once
#include "inspect_types.hpp"
#include <string>

namespace moex_fast {

// Generate deterministic JSON string from the report.
std::string report_to_json(const InspectionReport& report);

// Generate human-readable summary string.
std::string report_to_text(const InspectionReport& report);

// Write JSON report to file. Returns empty string on success, error message on failure.
std::string write_json_report(const InspectionReport& report, const std::string& path);

}  // namespace moex_fast
