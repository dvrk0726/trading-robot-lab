#pragma once
#include "inspect_types.hpp"
#include <string>

namespace moex_fast {

struct InspectorOptions {
    std::string configuration_path;
    std::string templates_path;
    std::string json_out_path;
    std::string profile;  // "auto" (default), "spectra-1.29", "spectra-1.30"
    bool strict{};
};

// Run the full inspection pipeline and return a report.
InspectionReport run_inspector(const InspectorOptions& opts);

}  // namespace moex_fast
